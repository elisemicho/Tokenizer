#include <iostream>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <onmt/Tokenizer.h>

namespace po = boost::program_options;

/*
int main(int argc, char* argv[])
{
  po::options_description desc("Tokenization with BPE");
  desc.add_options()
    ("help,h", "display available options")
    ("mode,m", po::value<std::string>()->default_value("conservative"), "Define how aggressive should the tokenization be: 'aggressive' only keeps sequences of letters/numbers, 'conservative' allows mix of alphanumeric as in: '2,000', 'E65', 'soft-landing'")
    ("joiner_annotate,j", po::bool_switch()->default_value(false), "include joiner annotation using 'joiner' character")
    ("joiner", po::value<std::string>()->default_value(onmt::Tokenizer::joiner_marker), "character used to annotate joiners")
    ("joiner_new", po::bool_switch()->default_value(false), "in joiner_annotate mode, 'joiner' is an independent token")
    ("case_feature,c", po::bool_switch()->default_value(false), "lowercase corpus and generate case feature")
    ("segment_case", po::bool_switch()->default_value(false), "Segment case feature, splits AbC to Ab C to be able to restore case")
    ("segment_numbers", po::bool_switch()->default_value(false), "Segment numbers into single digits")
    ("segment_alphabet", po::value<std::string>()->default_value(""), "comma-separated list of alphabets on which to segment all letters.")
    ("segment_alphabet_change", po::bool_switch()->default_value(false), "Segment if the alphabet changes between 2 letters.")
    ("bpe_model,bpe", po::value<std::string>()->default_value(""), "path to the BPE model")
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cerr << desc << std::endl;
    return 1;
  }

  int flags = 0;
  if (vm["case_feature"].as<bool>())
    flags |= onmt::Tokenizer::Flags::CaseFeature;
  if (vm["joiner_annotate"].as<bool>())
    flags |= onmt::Tokenizer::Flags::JoinerAnnotate;
  if (vm["joiner_new"].as<bool>())
    flags |= onmt::Tokenizer::Flags::JoinerNew;
  if (vm["segment_case"].as<bool>())
    flags |= onmt::Tokenizer::Flags::SegmentCase;
  if (vm["segment_numbers"].as<bool>())
    flags |= onmt::Tokenizer::Flags::SegmentNumbers;
  if (vm["segment_alphabet_change"].as<bool>())
    flags |= onmt::Tokenizer::Flags::SegmentAlphabetChange;

  std::vector<std::string> alphabets_to_segment;
  boost::split(alphabets_to_segment,
               vm["segment_alphabet"].as<std::string>(),
               boost::is_any_of(","));

  onmt::Tokenizer* tokenizer = new onmt::Tokenizer(
    onmt::Tokenizer::mapMode.at(vm["mode"].as<std::string>()),
    flags,
    vm["bpe_model"].as<std::string>(),
    vm["joiner"].as<std::string>());

  for (const auto& alphabet : alphabets_to_segment)
    tokenizer->add_alphabet_to_segment(alphabet);

  std::string line;

  while (std::getline(std::cin, line))
  {
    if (!line.empty())
      std::cout << reinterpret_cast<onmt::ITokenizer*>(tokenizer)->tokenize(line);

    std::cout << std::endl;
  }

  return 0;
}
*/

int main(int argc, char* argv[])
{
  bool verbose = 0;

  po::options_description desc("Tokenization with Morfessor");
  desc.add_options()
    ("mode,m", po::value<std::string>()->default_value("conservative"), "Define how aggressive should the tokenization be: 'aggressive' only keeps sequences of letters/numbers, 'conservative' allows mix of alphanumeric as in: '2,000', 'E65', 'soft-landing'")
    ("morfessor_model,morf", po::value<std::string>()->default_value(""), "path to the Morfessor model")
    ("joiner,j", po::value<std::string>()->default_value(onmt::Tokenizer::joiner_marker), "character used to annotate joiners (def '￭')")
    ("addcount,a", po::value<float>()->default_value(0), "additive smoothing (def 0:no smoothing)")
    ("maxlen,m", po::value<size_t>()->default_value(30), "maximum length for a subword (def 30)")
    ("nbest,n", po::value<size_t>()->default_value(0), "nbest size (def 0:output onebest)")
    ("beam,b", po::value<size_t>()->default_value(0), "beam size ONLY used when nbest>0 (def 0:no prunning)")
    ("help,h", "this help message")
    ("verbose,v", "verbose output")
    ;

  po::variables_map vm;

  try
  {
    po::store(po::parse_command_line(argc, argv, desc), vm); // can throw
    if (!vm.count("morfessor_model"))
    {
      std::cout << "error: missing -lexicon option" << std::endl;
      std::cout << desc << std::endl;
      return 1; // error
    }
    if (vm.count("help"))
    {
      std::cout << desc << std::endl;
      return 0; // success
    }
    if (vm.count("verbose"))
    {
      verbose=1;
    }
    // insert the condition if (nbest==0) beam=1;
    po::notify(vm);
  }
  catch(po::error& e)
  {
    std::cerr << "error: " << e.what() << std::endl << std::endl;
    std::cerr << desc << std::endl;
    return 1; // error
  }

  onmt::ITokenizer* tokenizer = new onmt::Tokenizer(onmt::Tokenizer::mapMode.at(vm["mode"].as<std::string>()),
                                                    false, //case_feature
                                                    false, //joiner_annotate
                                                    false, //joiner_new
                                                    vm["joiner"].as<std::string>(), //joiner
                                                    false, //with_separators
                                                    false, //segment_case
                                                    false, //segment_numbers
                                                    vm["morfessor_model"].as<std::string>(), //morfessor_model_path
                                                    false, //cache_morfessor_model
                                                    vm["addcount"].as<float>(), // addcount
                                                    vm["maxlen"].as<size_t>(), // maxlen
                                                    vm["nbest"].as<size_t>(), // nbest
                                                    vm["beam"].as<size_t>(), // beam
                                                    vm["verbose"].as<bool>() // verbose
  );

  std::string line;

  while (std::getline(std::cin, line))
    if (verbose) std::cout << "original: " << line << std::endl;
  {
    if (!line.empty())
      std::cout << tokenizer->tokenize(line);

    std::cout << std::endl;
  }

  return 0;
}

