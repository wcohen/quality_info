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

ostream *errfile;

bool inregister(localVar *j, VariableLocation k)
{
	return ((k.stClass==storageReg) && (k.refClass==storageNoRef));
}

int main(int argc, char **argv){
  //Name the object file to be parsed:
  std::string file;
  errfile=&cerr;
  SymtabCodeSource *sts;
  CodeObject *co;
  Symtab *syms = NULL;

  file=argv[1];
  if (file.length() == 0) {
	  exit(EXIT_NOFILE);
  }

  // Create a new binary code object from the filename argument
  sts = new SymtabCodeSource( argv[1] );
  if( !sts )
    exit(EXIT_MODULE);
  co = new CodeObject( sts );
  if( !co )
    exit(EXIT_MODULE);
  if(!Symtab::openFile(syms, argv[1]))
    exit(EXIT_MODULE);

  // Parse the binary
  co->parse();

  //iterate through each of the the functions
  for( auto f: co->funcs()) {
	  SymtabAPI::Function *func_sym;
	  if (!syms->findFuncByEntryOffset(func_sym, f->addr())) {
		  cerr << "unable to find " << f->name() << endl;
		  continue;
	  }
	  // interval containers for registers
	  std::map<MachRegister, interval_map<Address, varset> > register_loclist;
	  
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
	  
	  #if 1
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
	  #endif

	  // go through each block
	  for (auto b = f->blocks().begin(); b != f->blocks().end(); ++b) {
		  Block *bb = *b;
		  Block::Insns insns;

		  bb->getInsns(insns);
		  // go through each instruction in the block
		  for (auto insn_iter : insns) {
			  Instruction curInsn = insn_iter.second;
			  Address curAddr = insn_iter.first;

			  // construct a liveness query location
			  InsnLoc i(bb, curAddr, curInsn);
			  Location loc(f, i);
			  
			  // Query about variables that use register at instruction entry
			  for (auto it: register_loclist){
				  MachRegister reg = it.first;
				  bool live;

				  // if nothing in register at time, skip
				  auto range = it.second.find(curAddr);
				  if (range==it.second.end()){
					  continue;
				  }
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
}
