/*
 * common.h
 *
 * header for common code used by the various analysis tools
 * -handling of filters
 *
 */

typedef std::set<localVar *> varset;
typedef std::map<MachRegister, interval_map<Address, varset> > reg_locs;

enum exit_codes {
		 EXIT_OK=0,
		 EXIT_ARGS=1,
		 EXIT_MODULE=2,
		 EXIT_NOFUNCS=3,
		 EXIT_LFUNC=4,
		 EXIT_LF_NOTUNIQ=5,
		 EXIT_GLOBALS=6,
		 EXIT_NOFILE=7
};

struct session_info
{
  bool dbg_reglocs;
  bool dbg_filter;
  //Name the object file to be parsed:
  std::string file;
  vector<std::string> filters;
  SymtabCodeSource *sts;
  CodeObject *co;
  Symtab *syms;
  interval_map<Address, std::string > interest_reason;
};

extern std::string noreason;
extern bool inregister(localVar *j, VariableLocation k);
extern std::string interesting_loc(session_info &session, Address addr);
extern void dump_reg_intervals(reg_locs &register_loclist);
extern void track_func_variables(session_info &session,
				 reg_locs &register_loclist,
				 SymtabAPI::FunctionBase *func_sym);
extern void search_and_track_variables(session_info &session,
				       reg_locs &register_loclist,
				       SymtabAPI::FunctionBase *func_sym);

extern const char *argp_program_version;
extern const char *argp_program_bug_address;

extern exit_codes process_options(int argc, char **argv, session_info &session);
extern exit_codes process_filters(session_info &session);
extern exit_codes process_binaries(session_info &session);
