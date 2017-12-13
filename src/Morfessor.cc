#include <onmt/Morfessor.h>
#include <onmt/Tokenizer.h>
#include <fstream>
#include <iostream>

namespace onmt
{
  class Hyp
  {
  public:
    Hyp(float cost,std::vector<size_t>& sizes,size_t id){
      _cost=cost;
      _sizes=sizes;
      _id=id;
    }
    float _cost; //accumulated hypothesis cost
    std::vector<size_t> _sizes; //length of segments
    size_t _id; //just for debug use
  };

  class compareHyp
  {
  public:
    bool operator() (const Hyp& h1, const Hyp& h2) const{ 
      return h1._cost < h2._cost; 
    } 
  };

  Morfessor::Morfessor(const std::string& model_path)
    : beam (0)
    , nbest (0)
    , addcount (0) //constant for additive smoothing (0 = no smoothing)
    , maxlen (30) //maximum length for subwords
    , joiner (onmt::Tokenizer::joiner_marker)
    , verbose (0)
  {

    n_lexicon_subwords = 0; //number of words in lexicon -> lexicon_coding.boundaries
    std::string count_sep = " ";
    std::string pars_tag = "#";

    //boost::filesystem::ifstream file(model_path); // does not work
    std::ifstream file(model_path);
    if (!file.is_open())
      throw std::invalid_argument("Unable to open Morfessor model `" + model_path + "'");

    std::string str;

    // first line in morfessor model contains 3 options separated by ;
    std::vector<std::string> vstr;
    if (getline(file, str)){
      boost::split(vstr, str,boost::is_any_of(";"));
      if (vstr.size()!=3){
        std::cerr << "error: invalid number of parameters (first line) in model file" << std::endl;
        exit(1);
      }
    }
    else{
      std::cerr << "error: missing parameters (first line) in model file" << std::endl;
      exit(1);
    }

    n_corpus_subwords = atoi(vstr[0].c_str()); //number of subwords in model (weighted by freq) -> corpus_coding.tokens
    n_corpus_words = atoi(vstr[1].c_str()); //number of words in model (weighted by freq) -> corpus_coding.boundaries
    corpus_weight = atof(vstr[2].c_str());

    while (getline(file, str)){
      size_t pos;
      pos = str.find(pars_tag);
      if (pos==0){
        str = str.substr(pars_tag.size());
        pos = str.find(count_sep);
        std::string name = str.substr(0,pos);
        std::string val = str.substr(pos+count_sep.size());
        if      (name == "corpus_coding.tokens")     {n_corpus_subwords = atoi(val.c_str());}
        else if (name == "corpus_coding.boundaries") {n_corpus_words = atoi(val.c_str());}
        else if (name == "corpus_coding.weight")     {corpus_weight = atof(val.c_str());}
        continue;
      }
      size_t freq = 1;
      std::string subword = str;
      pos = str.find(count_sep);
      if (pos!=std::string::npos) {
        std::string strfreq = str.substr(0,pos);
        freq = atoi(strfreq.c_str());
        subword = str.substr(pos+count_sep.size());
      }
      boost::unordered::unordered_map<std::string,size_t>::iterator it = subword2freq.find(subword);
      if (it == subword2freq.end()) subword2freq[subword] = freq;
      else it->second += freq;
      n_lexicon_subwords++;
    }

    if (verbose){
      std::cout << "corpus_coding.tokens " << n_corpus_subwords << std::endl;
      std::cout << "corpus_coding.boundaries " << n_corpus_words << std::endl;
      std::cout << "corpus_coding.weight " << corpus_weight << std::endl;
      std::cout << "lexicon_coding.boundaries " << n_lexicon_subwords << " (lexicon entries)" << std::endl;
      std::cout << "maxlen " << maxlen << std::endl;
      std::cout << "addcount " << addcount << std::endl;
      std::cout << "beam " << beam << std::endl;
      std::cout << "nbest " << nbest << std::endl;
      std::cout << "joiner '" << joiner << "'" << std::endl;
    }
  }

  std::vector<std::pair<float, std::string> > Morfessor::segment(const std::string &word) const
  {
    if (verbose) std::cout << "segment(" << word << ")" << std::endl;

    UnicodeString ucword = UnicodeString::fromUTF8(StringPiece(word.c_str())); //to split words i need to work with UnicodeString's
    size_t word_length = ucword.length();

    std::vector<std::multiset<Hyp, compareHyp> > Hyps; //search space
    for (size_t from = 0; from <= word_length; from++){ //hypotheses covering up to length from, will be stored in Hyps[from]
      std::multiset<Hyp, compareHyp> list;
      Hyps.push_back(list);
    }
    //empty hypothesis to start the search
    size_t hypid = 0;
    std::vector<size_t> v;
    Hyp hempty(0.0, v, hypid++);
    Hyps[0].insert(hempty);
    
    float bestcostfinalhyp;
    //beam search
    for (size_t from = 0; from < word_length; from++){ //next subword starts at from
      
      if (verbose) std::cout << "\tExpanding hyps in list=" << from << " nhyps=" << Hyps[from].size() << std::endl;

      for (size_t to = from; to < word_length && (maxlen == 0 || to-from+1 <= maxlen); to++){ //subword is [from,to]
        UnicodeString ucsubword;
        ucword.extract(from, to-from+1, ucsubword);
        std::string subword;
        ucsubword.toUTF8String(subword);
        bool skip = false;
        float cost = get_cost(to-from+1, word_length, subword, skip);
        if (skip) continue;

        if (verbose){
          boost::unordered::unordered_map<std::string,size_t>::const_iterator it_subword2freq = subword2freq.find(subword);
          std::cout << "\t\tword[" << from << "," << to << "]='" << subword << "' freq=" << (it_subword2freq == subword2freq.end()?0:it_subword2freq -> second) << std::endl;
        }
    
        size_t n=0; //n hypothesis to be expanded in Hyps[from]
        for (std::multiset<Hyp, compareHyp>::iterator it_Hyps=Hyps[from].begin(); it_Hyps!=Hyps[from].end(); it_Hyps++){ //expand hyps in Hyps[from] with current subword
          std::vector<size_t> fathersizes = (*it_Hyps)._sizes;
          float fathercost = (*it_Hyps)._cost;
          float currcost = fathercost + cost;
          
          //i do not add the hypothesis if one-best and its cost is worst than the best final hyp (and keep track of the best final hypothesis cost)
          if (nbest == 0){
            if (Hyps[word_length].size() && bestcostfinalhyp < currcost) continue;
            if (to+1 == word_length && (Hyps[word_length].size() == 0 || currcost < bestcostfinalhyp)) bestcostfinalhyp = currcost;
          }

          //insert new hypothesis
          std::vector<size_t> currsizes = fathersizes;
          currsizes.push_back(to-from+1);
          Hyp currh(currcost, currsizes, hypid++);
          Hyps[to+1].insert(currh);

          if (verbose){
            std::cout << "\t\t\t" << " father:" << (*it_Hyps)._id << " hyp:" << currh._id << " [";
            for (size_t i = 0; i < currsizes.size(); i++){std::cout << " " << currsizes[i];}
            std::cout << " ] cost=" << currcost << (to == word_length-1?" [FINAL]":"") << std::endl;
          }
          n++;
          if (beam > 0 && n == beam) break;
        }
      }
    }
      
    std::vector<std::pair<float,std::string> > segmentations;
    std::multiset<Hyp, compareHyp>::iterator it_Hyps = Hyps[word_length].begin();
    for (std::multiset<Hyp, compareHyp>::iterator it_Hyps = Hyps[word_length].begin(); it_Hyps!=Hyps[word_length].end(); it_Hyps++){
      std::vector<size_t> sizes = (*it_Hyps)._sizes;
      std::stringstream segmentation;
      size_t from = 0;
      for (size_t i = 0; i<sizes.size(); i++){
        UnicodeString ucsubword; 
        ucword.extract(from, sizes[i], ucsubword);
        std::string subword;
        ucsubword.toUTF8String(subword);
        segmentation << (i?joiner:"") << subword;
        from += sizes[i];
      }
      std::pair<float,std::string> p = std::make_pair((*it_Hyps)._cost, segmentation.str());
      segmentations.push_back(p);
      if (nbest == 0 || segmentations.size() == nbest) break;
    }
    return segmentations;
  }

  float Morfessor::get_cost(size_t subword_length, size_t word_length, std::string& subword, bool& skip) const
  {
    float cost = 0;
    size_t freq = 0;
    boost::unordered::unordered_map<std::string,size_t>::const_iterator it_subword2freq = subword2freq.find(subword);
    if (it_subword2freq != subword2freq.end()) freq = it_subword2freq -> second;
    float logtokens = 0;
    if (n_corpus_subwords + n_corpus_words + addcount > 0) logtokens = log(n_corpus_subwords + n_corpus_words + addcount);
    float badlikelihood = word_length*logtokens + 1.0; //worst cost

    if (freq>0){ // subword exists     
      //      cost = log(freq + addcount) - logtokens;
      cost = logtokens - log(freq + addcount);
    }
    else if (addcount>0){ //smoothing applies
      if (n_corpus_subwords==0){
        cost = ( addcount * log(addcount) + subword_length / corpus_weight );
      }
      else{
        cost = ( logtokens - log(addcount) + (((n_lexicon_subwords+addcount) * log(n_lexicon_subwords+addcount) ) - ( n_lexicon_subwords*log(n_lexicon_subwords)) + subword_length) / corpus_weight);
      }
    }
    else if (subword_length == 1) cost=badlikelihood;
    else skip=true; //not a valid segment
    return cost;
  }
}
