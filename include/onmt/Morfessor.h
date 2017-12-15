#pragma once

#include <string>
#include <boost/unordered_map.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <unicode/ustdio.h>
#include <boost/algorithm/string.hpp>
#include <vector>

namespace onmt
{

  class Morfessor
  {
  public:
    Morfessor(const std::string& model_path);

    std::vector<std::pair<float, std::string> > segment(const std::string &word) const;
    
  private:
    boost::unordered::unordered_map<std::string,size_t> subword2freq; //'Afternoon' => 4
    size_t n_corpus_words;
    size_t n_corpus_subwords;
    size_t n_lexicon_subwords;
    float corpus_weight;
    size_t beam;
    size_t nbest;
    float addcount; //constant for additive smoothing (0 = no smoothing)
    size_t maxlen; //maximum length for subwords
    std::string joiner;
    bool verbose;

    float get_cost(size_t subword_length, size_t word_length, std::string& subword, bool& skip) const;
  };

}