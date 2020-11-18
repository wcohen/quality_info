/*
 * cse.C
 *
 * cse is an attempt to get some information about multiple variables
 * are share the same value at various places in the binary.
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


	  std::map<MachRegister, interval_map<Address, varset> >::iterator it;
	  for (it=register_loclist.begin(); it!=register_loclist.end(); ++it){
		  // print out the location lists associated
		  interval_map<Address, varset>::iterator it2 = it->second.begin();
		  while(it2 != it->second.end())
		  {
			  // skip if there is 1 or fewer variables in register
			  if (it2->second.size() > 1) {
				  cout << f->name() << endl;
				  interval<Address>::type where = it2->first;
				  // What variables in register in Address interval?
				  cout << std::hex << where << " " << it->first.name();
				  cout << " ";
				  std::set<localVar *>::iterator it3 = it2->second.begin();
				  while(it3 != it2->second.end()) {
					  cout << (*it3)->getName() << " ";
					  ++it3;
				  }
				  cout << endl;
			  }
			  ++it2;
		  }
	  }
	  
  }
}
