#include <iostream>
#include <memory>
#include <list>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <bitset>
#include <stack>

#include <utils/io/smart_ifstream.hpp>

using namespace std;

struct State {

  bool is_finalize;
  string token_id;

  State(bool is_finalize = false)
  : is_finalize(is_finalize)
  {}

  ~State() {}

  void set_finalize(string &id) {
    is_finalize = true;
    token_id = id;
  }
};

char translate_escape_character(char escape_character) {
  static unordered_map<char, char> translation_map = {
    { 'b', ' ' }, { 't','\t' }, { 'n','\n' }, { '[', '[' }, { ']', ']' }, 
    { '.', '.' }, {'\\','\\' },
  };
  if (translation_map.count(escape_character) > 0) {
    return translation_map[escape_character];
  } else {
    return '\0';
  }
}

bitset<256> character_set_all() {
  bitset<256> character_set = 0;
  character_set.set();
  return character_set;
}

bitset<256> parse_character_set_without_reverse(const string& pattern) {
  bitset<256> character_set = 0;
  for (std::size_t idx = 0, length = pattern.length(); idx < length; ++idx) {
    if (pattern[idx] == '\\') {
      character_set.set(translate_escape_character(pattern[idx + 1]));
      ++idx;
    } else if (isdigit(pattern[idx]) && pattern[idx + 1] == '-' && idx + 2 < length && isdigit(pattern[idx + 2])) {
      for (auto ch = pattern[idx]; ch <= pattern[idx + 2]; ++ch) {
        character_set.set(ch);
      }
      idx += 2;
    } else if (isalpha(pattern[idx]) && pattern[idx + 1] == '-' && idx + 2 < length && isalpha(pattern[idx + 2])) {
      for (auto ch = pattern[idx]; ch <= pattern[idx + 2]; ++ch) {
        character_set.set(ch);
      }
      idx += 2;
    } else {
      character_set.set(pattern[idx]);
    }
  }
  return character_set;
}

bitset<256> parse_character_set(const string& pattern) {
  bitset<256> character_set = 0;
  if (pattern[0] == '^') {
    if (pattern.length() > 1) {
      character_set = ~parse_character_set_without_reverse(pattern.substr(1));
    }
  } else {
    character_set = parse_character_set_without_reverse(pattern);
  }
  return character_set;
}

struct Transition {

  shared_ptr<State> source, target;
  bool is_epsilon = false;
  bitset<256> character_set = 0;

  Transition(shared_ptr<State> source, shared_ptr<State> target, const string& parameter)
  : source(source), target(target)
  {
    if (parameter == "epsilon") {
      is_epsilon = true;
    } else if (parameter.length() == 1) { // single character
      if (parameter[0] == '.') {
        character_set = character_set_all();
      } else {
        character_set.set(parameter[0]);
      }
    } else if (parameter.length() == 2 && parameter[0] == '\\') { // escape character
      character_set.set(translate_escape_character(parameter[1]));
    } else if (parameter[0] == '[' && parameter.back() == ']') { // char range
      character_set = parse_character_set(parameter.substr(1, parameter.length() - 2));
    }
  }

  ~Transition() {}

  bool accept(char ch) {
    return character_set[ch];
  }

  void output_acceptable_characters() {
    for (int ch = 0; ch <= 256; ++ch) {
      if (character_set[ch] && isprint(ch)) {
        std::cout << ch;
      }
    }
  }
};

class Automaton {

public:
  Automaton() {
    m_start_state = make_shared<State>();
    m_states.insert(m_start_state);
  }

  ~Automaton() {}

  bool add_state(shared_ptr<State> state) {
    if (m_states.count(state) > 0) {
      return false;
    } else {
      m_states.insert(state);
      return true;
    }
  }

  size_t num_states() {
    return m_states.size();
  }

  unordered_set<shared_ptr<State>> states() {
    unordered_set<shared_ptr<State>> states(m_states.begin(), m_states.end());
    return states;
  }

  shared_ptr<State> start_state() {
    return m_start_state;
  }

  bool add_transition(shared_ptr<State> source, shared_ptr<State> target, const string &parameter) {
    if (m_states.count(source) > 0 && m_states.count(target) > 0) {
      shared_ptr<Transition> transition = make_shared<Transition>(source, target, parameter);
      m_transitions.insert(transition);
      m_sources[target].emplace_back(transition);
      m_targets[source].emplace_back(transition);
      return true;
    } else {
      return false;
    }
  }

  size_t num_transitions() {
    return m_transitions.size();
  }

  unordered_set<shared_ptr<Transition>> transitions() {
    unordered_set<shared_ptr<Transition>> transitions(m_transitions.begin(), m_transitions.end());
    return transitions;
  }

  unordered_set<shared_ptr<Transition>> available_transitions(shared_ptr<State> state) {
    unordered_set<shared_ptr<Transition>> transitions;
    for (auto &target: m_targets[state]) {
      transitions.emplace(target);
    }
    return transitions;
  }

private:
  shared_ptr<State> m_start_state;
  unordered_set<shared_ptr<State>> m_states;
  unordered_set<shared_ptr<Transition>> m_transitions;

  unordered_map<shared_ptr<State>, list<weak_ptr<Transition>>> m_sources;
  unordered_map<shared_ptr<State>, list<weak_ptr<Transition>>> m_targets;
};

std::string smart_character_output(char c) {
  if (c == ' ') {
    return "\\b";
  } else if (c == '\t') {
    return "\\t";
  } else if (c == '\n') {
    return "\\n";
  } else {
    return std::string { c };
  }
}

void smart_token_output(const string& token_id, const string& accepted_string) {
  cout << "< " << token_id << " , ";
  for (auto& ch: accepted_string) {
    cout << smart_character_output(ch);
  }
  cout << " >\n";
}

void scan(shared_ptr<Automaton> dfa, const std::string &input) {
  size_t i_start = 0, i_offset;
  for ( ; i_start < input.length(); i_start += i_offset) {
    stack<shared_ptr<State>> stack;
    stack.push(dfa->start_state());
    i_offset = 1;
    while (i_start + i_offset <= input.length() && !stack.empty()) {
      auto current_state = stack.top();
      bool found = false;
      for (auto &transition: dfa->available_transitions(current_state)) {
        if (transition->accept(input[i_start + i_offset - 1])) {
          found = true;
          stack.push(transition->target);
          break;
        }
      }
      if (found) {
        i_offset += 1;
      } else {
        i_offset -= 1;
        break;
      }
    }
    while (i_offset > 0 && !stack.top()->is_finalize) {
      i_offset -= 1;
      stack.pop();
    }
    if (i_offset == 0) {
      smart_token_output("INVALID", input.substr(i_start, 1));
      i_start += 1;
    } else {
      auto stopped_state = stack.top();
      if (stopped_state->token_id != "BLANK") {
        smart_token_output(stopped_state->token_id, input.substr(i_start, i_offset));
      }
    }
  }
}

int main(int argc, char *argv[]) {
  using utils::io::smart_ifstream;
  smart_ifstream smart_in("assets/dfa/dfa_define.txt");

  shared_ptr<Automaton> dfa = make_shared<Automaton>();

  size_t num_states, num_finalize_states;
  smart_in >> num_states >> num_finalize_states;

  // create all dfa states
  vector<shared_ptr<State>> states { dfa->start_state() };
  // bypass start state, which is indexed 0 and created when dfa is constructed
  for (size_t i = 1; i < num_states; ++i) {
    auto state = make_shared<State>();
    states.push_back(state);
    dfa->add_state(state);
  }

  // set finilize states
  while (num_finalize_states--) {
    size_t idx_state;
    string token_id;
    smart_in >> idx_state >> token_id;
    states[idx_state]->set_finalize(token_id);
  }

  // create all transitions
  {
    size_t idx_state_in, idx_state_out;
    string parameter;
    while (smart_in >> idx_state_in >> idx_state_out >> parameter) {
      dfa->add_transition(states[idx_state_in], states[idx_state_out], parameter);
    }
  }

  for (size_t i = 0; i < num_states; ++i) {
    
  }

  scan(dfa, "if (x==1) x = x + 1;\n");

  return 0;
}