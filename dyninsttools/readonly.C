/*
 * readonly.C
 *
 * readonly is an attempt to get some information about which variables
 * are readonly at various places in the binary.
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

#include <argp.h>
#include <map>
#include <boost/icl/interval_map.hpp>

#include <dyninst/Symtab.h>
#include <dyninst/CodeSource.h>
#include <dyninst/Location.h>
#include <dyninst/Function.h>
#include <dyninst/liveness.h>
#include <dyninst/bitArray.h>

using namespace std;
using namespace Dyninst;
using namespace SymtabAPI;
using namespace ParseAPI;
using namespace boost::icl;

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
  //Name the object file to be parsed:
  std::string file;
  vector<std::string> filters;
  SymtabCodeSource *sts;
  CodeObject *co;
  Symtab *syms;
  interval_map<Address, std::string > interest_reason;
};

static string noreason("");
static string reason("reason");

bool inregister(localVar *j, VariableLocation k)
{
	return ((k.stClass==storageReg) && (k.refClass==storageNoRef));
}

string interesting_loc(session_info &session, Address addr)
{
	auto info=session.interest_reason.find(addr);
	if (info==session.interest_reason.end()) {
		return noreason;
	}
	return info->second;
}

void dump_reg_intervals(reg_locs &register_loclist)
{
	cout << "dump of address intervals" << endl;
	for (auto it: register_loclist) {
		cout << "register " << it.first.name() << endl;
		// print out the location lists associated
		for (auto it2: it.second) {
			interval<Address>::type where = it2.first;
			// What variables in register in Address interval?
			cout << std::hex << where << ": ";
			// print out the variables
			for (auto it3: it2.second) {
				cout << (it3)->getName() << " ";
			}
			cout << endl;
		}
	}
	cout << "end of dump" << endl;
}

extern const char *argp_program_version;
extern const char *argp_program_bug_address;

/* Program documentation. */
extern char doc[];

/* A description of the arguments we accept. */
static char args_doc[] = "INPUT_FILE";

/* The options we understand. */
static struct argp_option options[] = {
  {"debug",  'd', 0,      0,  "Produce additional debug output" },
  {"filter",  'f', "FILTER", 0,  "Limit analysis to items that match filter" },
  { 0 }
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
	struct session_info *arguments = (struct session_info *) state->input;

  switch (key)
    {
    case 'd':
      arguments->dbg_reglocs = true;
      break;

    case 'f':
      arguments->filters.push_back(arg);
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 1)
        /* Too many arguments. */
        argp_usage (state);

      arguments->file = arg;

      break;

    case ARGP_KEY_END:
      if (state->arg_num < 1)
        /* Not enough arguments. */
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

exit_codes process_options(int argc, char **argv, session_info &session)
{
  /* Set default values. */
  session.dbg_reglocs = false;

  /* Figure out settings to put into session */
  argp_parse (&argp, argc, argv, 0, 0, &session);

  return(EXIT_OK);
}

exit_codes process_binaries(session_info &session)
{
  // Create a new binary code object from the filename argument
  session.sts = new SymtabCodeSource( strdup(session.file.c_str()) );
  if( !session.sts )
    return(EXIT_MODULE);
  session.co = new CodeObject( session.sts );
  if( !session.co )
    return(EXIT_MODULE);
  if(!Symtab::openFile(session.syms, session.file))
    return(EXIT_MODULE);

  // Parse the binary
  session.co->parse();
  return(EXIT_OK);
}

void filter_function(session_info &session, string func, Address start, Address end)
{
	cout << "filter_function: " << func << " [" << hex << start << "," << hex << end << ")" << endl;
	session.interest_reason.add(make_pair(interval<Address>::right_open(start, end), func));
	return;
}

/* Default filter:
   Start of each regular function
   Start of each inlined function (need to determine which variables are args to function )
 */
void default_filters(session_info &session)
{
	/* Right now mark the entire regular function */
	for( auto f: session.co->funcs()) {
	  SymtabAPI::Function *func_sym;
	  if (!session.syms->findFuncByEntryOffset(func_sym, f->addr())) {
		  cerr << "unable to find " << f->name() << endl;
		  continue;
	  }
	  filter_function(session, func_sym->getName(), f->addr(), f->addr()+func_sym->getSize());
	}
	return;
}

exit_codes process_filters(session_info &session)
{
	/* Go through each of the filters and see which addresses match */
	for (auto filter : session.filters) {
		cout << "filter: " << filter << endl;
		/* line numbers */
		/* function entry */
		/* function return */
	}
	/* if no filters, just make default ones */
	cout << "session.filters.size() " << session.filters.size() << endl;
	if (session.filters.size() == 0) {
		default_filters(session);
	}
	return(EXIT_OK);
}

/* Code above will go in library and code below will stay in this file */

const char *argp_program_version = "readonly 0.1";
const char *argp_program_bug_address = "https://github.com/wcohen/quality_info/issues";

/* Program documentation. */
char doc[] = "readonly -- examine binary and debuginfo to determine where writes to variables have no effect";

exit_codes analyze_binaries(session_info &session)
{
  //iterate through each of the the functions
  for( auto f: session.co->funcs()) {
	  SymtabAPI::Function *func_sym;
	  if (!session.syms->findFuncByEntryOffset(func_sym, f->addr())) {
		  cerr << "unable to find " << f->name() << endl;
		  continue;
	  }
	  // interval containers for registers
	  reg_locs register_loclist;
	  
	  // now get list of locations for function
	  vector <localVar *> lvars;
	  func_sym->getParams(lvars);
	  func_sym->getLocalVariables(lvars);
	  for(auto j: lvars) {
	         vector<VariableLocation> &lvlocs=j->getLocationLists();
		 for(auto k: lvlocs) {
			 if (inregister(j,k)) {
				 // enter info for location list
				 varset a_loc;
				 a_loc.insert(j);
				 register_loclist[k.mr_reg].add(make_pair(interval<Address>::right_open(k.lowPC, k.hiPC), a_loc));
			 }
		 }
	  }

	  LivenessAnalyzer la(f->obj()->cs()->getAddressWidth());

	  la.analyze(f);

	  if (session.dbg_reglocs)
		  dump_reg_intervals(register_loclist);

	  // go through each block
	  for (auto b = f->blocks().begin(); b != f->blocks().end(); ++b) {
		  Block *bb = *b;
		  Block::Insns insns;

		  bb->getInsns(insns);
		  // go through each instruction in the block
		  for (auto insn_iter : insns) {
			  Instruction curInsn = insn_iter.second;
			  Address curAddr = insn_iter.first;

			  // Determine whether this is a location care about
			  string point_of_interest = interesting_loc(session, curAddr);
			  if (point_of_interest == noreason)
				  continue;

			  // construct a liveness query location
			  InsnLoc i(bb, curAddr, curInsn);
			  Location loc(f, i);

			  bool printed_reason = false;
			  
			  // Query about variables that use register at instruction entry
			  for (auto it: register_loclist){
				  MachRegister reg = it.first;
				  bool live;

				  // if nothing in register at time, skip
				  auto range = it.second.find(curAddr);
				  if (range==it.second.end())
					  continue;

				  if (!la.query(loc, LivenessAnalyzer::Before, reg, live)) {
					  printf("Cannot look up live registers at instruction entry\n");
				  }

				  /* print out info about filter match */
				  if (!live) {
					  if (!printed_reason) {
						  printf("%s pc=0x%x\n", point_of_interest.c_str(),
							 insn_iter.first);
						  printed_reason = true;
					  }
					  // value not used again, so it is read only
					  printf("  %s readonly ",
						 reg.name().c_str());
					  for (auto it3: range->second) {
						  cout << (it3)->getName() << " ";
					  }
					  cout << endl;
				  }
			  }
		  }
	  }
  }
  return(EXIT_OK);
}

int main(int argc, char **argv){
  session_info session;

  exit_codes options_status = process_options(argc, argv, session);
  if ( options_status != EXIT_OK)
	  exit(options_status);

  exit_codes binaries_status = process_binaries(session);
  if ( binaries_status != EXIT_OK)
	  exit(binaries_status);

  exit_codes filters_status = process_filters(session);
  if ( filters_status != EXIT_OK)
	  exit(filters_status);

  exit_codes analyze_status = analyze_binaries(session);
  exit(analyze_status);
}
