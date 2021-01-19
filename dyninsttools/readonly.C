/*
 * readonly.C
 *
 * readonly is an attempt to get some information about which variables
 * are readonly at various places in the binary.
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

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

class session_info
{
public:
  session_info();
  bool dbg_reglocs;
  //Name the object file to be parsed:
  std::string file;
  SymtabCodeSource *sts;
  CodeObject *co;
  Symtab *syms;
};

session_info::session_info():
	dbg_reglocs(false),
	syms(NULL)
{
}

bool inregister(localVar *j, VariableLocation k)
{
	return ((k.stClass==storageReg) && (k.refClass==storageNoRef));
}

bool interesting_loc(session_info &session, Address addr)
{
	return true;
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


exit_codes process_options(int argc, char **argv, session_info &session)
{
  session.file=argv[1];
  if (session.file.length() == 0) {
	  exit(EXIT_NOFILE);
  }
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

	  printf("# %s\n", f->name().c_str());
	  la.analyze(f);

	  bool printed_name = false;

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

			  // FIXME Determine whether this is a location care about
			  if (!interesting_loc(session, curAddr))
				  continue;

			  // construct a liveness query location
			  InsnLoc i(bb, curAddr, curInsn);
			  Location loc(f, i);
			  
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

				  if (!live) {
					  // value not used again, so it is read only
					  printf("%x %s readonly ",
						 insn_iter.first,
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

  exit_codes analyze_status = analyze_binaries(session);
  exit(analyze_status);
}
