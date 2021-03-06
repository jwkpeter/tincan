#include "dbcparser.h"


#include <cstdint>
#include <tuple>
#include <utility>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <experimental/filesystem>

#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include "file/dbcmetaparser.h"


BOOST_FUSION_ADAPT_STRUCT(
  dbc::Value_def,
  (std::uint32_t, frame_id)
  (std::string, signal_name)
  (dbc::Value_map, value_definitions)
)


BOOST_FUSION_ADAPT_STRUCT(
  dbc::Signal_def,
  (std::string, name)
  (std::int32_t, multiplex_value)
  (bool, multiplex_switch)
  (std::uint32_t, pos)
  (std::uint32_t, len)
  (dbc::Byte_order, order)
  (dbc::Value_sign, sign)
  (double, factor)
  (double, offset)
  (double, minimum)
  (double, maximum)
  (std::string, unit)
  (std::vector<std::string>, receiver)
  (dbc::Value_map, value_definitions)
  (dbc::Signal_meta_data, meta_data)
)


BOOST_FUSION_ADAPT_STRUCT(
  dbc::Frame_def,
  (std::uint32_t, id)
  (std::string, name)
  (std::uint32_t, dlc)
  (std::string, transmitter)
  (bool, multiplexer)
  (bool, multiplexer_extended)
  (std::vector<dbc::Signal_def>, signal_defs)
)


namespace fsys = std::experimental::filesystem;
namespace x3 = boost::spirit::x3;
namespace ascii = boost::spirit::x3::ascii;
namespace latin1 = boost::spirit::x3::iso8859_1;


namespace {


inline dbc::Frame_def* get_def(std::vector<dbc::Frame_def>& frame_defs, std::uint32_t id)
{
  auto it = std::find_if(std::begin(frame_defs), std::end(frame_defs),
      [id](const auto& m){ return m.id == id; });
  if (it != std::end(frame_defs))
    return &*it;
  return nullptr;
}


inline dbc::Signal_def* get_def(dbc::Frame_def* f, std::string_view signal_name)
{
  if (!f)
    return nullptr;
  auto it = std::find_if(std::begin(f->signal_defs), std::end(f->signal_defs),
      [signal_name](const auto& s){ return s.name == signal_name; });
  if (it != std::end(f->signal_defs))
    return &*it;
  return nullptr;
}


inline dbc::Signal_def* get_def(std::vector<dbc::Frame_def>& frame_defs, std::uint32_t frame_id,
  std::string_view signal_name)
{
  return get_def(get_def(frame_defs, frame_id), signal_name);
}


struct signs_ : x3::symbols<dbc::Value_sign>
{
  signs_()
  {
    add
      ("+", dbc::Value_sign::Unsigned)
      ("-", dbc::Value_sign::Signed)
    ;
  }
} signs;


struct oders_ : x3::symbols<dbc::Byte_order>
{
  oders_()
  {
    add
      ("0", dbc::Byte_order::Moto)
      ("1", dbc::Byte_order::Intel)
    ;
  }
} orders;


}  // namspace


namespace parsers {


using x3::char_;
using x3::long_;
using x3::ulong_;
using x3::double_;
using x3::lit;
using x3::attr;
using x3::lexeme;
using x3::omit;


// Black magic
template<typename T> static auto as = [](auto p){ return x3::rule<struct tag, T>{"as"} = p; };


const auto identifier = +char_("a-zA-Z0-9_");
const auto multiplex_indicator = (('m' >> long_) | attr(-1)) >> (('M' >> attr(true)) | attr(false));
const auto signal_name = lexeme[identifier];
const auto quoted_string = lexeme['"' >> *(latin1::char_ - '"') >> '"'];
const auto value_pair = as<std::pair<std::int32_t, std::string>>(long_ >> omit[+ascii::space]
    >> quoted_string);


x3::rule<class signal, dbc::Signal_def> const signal = "signal";
const auto signal_def =
    lit("SG_")
    >> signal_name >> multiplex_indicator >> ':'
    >> ulong_ >> '|' >> ulong_ >> '@'
    >> orders >> signs
    >> '(' >> double_ >> ',' >> double_ >> ')'
    >> '[' >> double_ >> '|' >> double_ >> ']'
    >> quoted_string
    >> identifier % ','
    >> attr(dbc::Value_map{})
    >> attr(dbc::Signal_meta_data{})
    ;
BOOST_SPIRIT_DEFINE(signal)


x3::rule<class frame, dbc::Frame_def> const frame = "frame";
const auto frame_def = lit("BO_") >> ulong_ >> identifier >> ':' >> ulong_ >> identifier
  >> attr(false) >> attr(false) >> attr(std::vector<dbc::Signal_def>{});
BOOST_SPIRIT_DEFINE(frame)


x3::rule<class value_def, dbc::Value_def> const value_def = "value_def";
const auto value_def_def = lit("VAL_") >> ulong_ >> signal_name
    >> lexeme[value_pair % omit[+ascii::space]];
BOOST_SPIRIT_DEFINE(value_def)


}  // namespace parsers


namespace {


inline void parse_nodes(std::string_view line, std::vector<std::string>& nodes)
{
  x3::phrase_parse(std::begin(line), std::end(line),
      x3::lit("BU_:") >> parsers::identifier % ',', ascii::space, nodes);
}


std::tuple<bool, dbc::Value_def> parse_value_def(std::string_view line)
{
  dbc::Value_def value_def;
  bool success = x3::phrase_parse(std::begin(line), std::end(line), parsers::value_def_def,
      ascii::space, value_def);
  return {success, value_def};
}


std::tuple<bool, dbc::Signal_def> parse_signal_def(std::string_view line)
{
  dbc::Signal_def signal;
  bool success = x3::phrase_parse(std::begin(line), std::end(line), parsers::signal, ascii::space,
      signal);
  return {success, signal};
}


std::tuple<bool, dbc::Frame_def> parse_frame_def(std::string_view line)
{ 
  dbc::Frame_def message;
  bool success = x3::phrase_parse(std::begin(line), std::end(line), parsers::frame, ascii::space,
      message);
  return {success, message};
}


[[maybe_unused]] inline bool messages_block_done(std::string_view line)
{
  // We're not interested in comments, etc.
  using latin1::space;
  using x3::lit;
  return x3::parse(std::begin(line), std::end(line), *space
      >> (lit("CM_") | lit("BA_DEF_") | lit("BA_")) >> space);
}


[[maybe_unused]] void sort_signals(dbc::Frame_def& frame_def)
{
  std::sort(std::begin(frame_def.signal_defs), std::end(frame_def.signal_defs),
      [](const auto& a, const auto& b){ return a.pos < b.pos; });
}


void mark_multiplexer(std::vector<dbc::Frame_def>& frame_defs)
{
  // Frames with multiple switch signals or multiplexed switch signals are extended multiplexer
  for (auto& fd : frame_defs) {
    int switch_signals = 0;
    for (const auto& sd : fd.signal_defs) {
      if (sd.multiplex_switch)
        switch_signals++;
      if (!fd.multiplexer_extended && sd.multiplex_switch && sd.multiplex_value > -1)
        fd.multiplexer_extended = true;
    }
    fd.multiplexer = switch_signals > 0;
    if (!fd.multiplexer_extended)
      fd.multiplexer_extended = switch_signals > 1;
  }
}


}  // namespace


dbc::File dbc::parse(std::string_view filepath)
{
  File dbc_file;
  std::vector<Value_def> value_definitions;

  std::ifstream fs{std::string{filepath}};
  if (!fs.is_open())
    throw Parse_error{"Could not open file"};

  dbc_file.name = fsys::path{filepath}.filename().string();

  std::string line;
  while (std::getline(fs, line)) {
    if (dbc_file.nodes.empty())
      parse_nodes(line, dbc_file.nodes);
    
    if (auto [success, signal_def] = parse_signal_def(line); success) {
      if (dbc_file.frame_defs.empty()) {
        throw Parse_error{"Signal defined before frame"};
      }
      else {
        dbc_file.frame_defs.back().signal_defs.push_back(signal_def);
        dbc_file.frame_defs.back().signal_defs.back().meta_data = dbc::meta::parse_signal(line);
      }
    }
    else if (auto [success, frame_def] = parse_frame_def(line); success) {
      dbc_file.frame_defs.push_back(frame_def);
    }
    else if (auto [success, value_def] = parse_value_def(line); success) {
      value_definitions.push_back(std::move(value_def));
    }
  }

  for (auto& v : value_definitions) {
    if (auto* signal_def = get_def(dbc_file.frame_defs, v.frame_id, v.signal_name); signal_def)
      signal_def->value_definitions = std::move(v.value_definitions);
  }

  mark_multiplexer(dbc_file.frame_defs);
  for (auto& fd : dbc_file.frame_defs) {
    if (fd.multiplexer_extended)
      throw Parse_error{"Extended multiplexing not supported"};
  }

  return dbc_file;
}
