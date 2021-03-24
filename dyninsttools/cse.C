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

#include "common.h"

const char *argp_program_version = "cse 0.1";
const char *argp_program_bug_address = "https://github.com/wcohen/quality_info/issues";

/* Program documentation. */
char doc[] = "cse -- examine binary and debuginfo to determine where mulitple variables share a register";

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
	  std::map<MachRegister, interval_map<Address, varset> > register_loclist;
	  
	  /* Track variables for the function (and any inlined functions it contains)*/
	  search_and_track_variables(session, register_loclist, func_sym);

	  bool printed_name = false;
	  for (auto it: register_loclist){
		  for(auto it2: it.second){
			  // skip if there is 1 or fewer variables in register
			  if (it2.second.size() > 1) {
				  if (!printed_name) {
					  cout << f->name() << endl;
					  printed_name = true;
				  }
				  interval<Address>::type where = it2.first;
				  // What variables in register in Address interval?
				  cout << std::hex << where << " " << it.first.name();
				  cout << " ";
				  for(auto it3: it2.second){
					  cout << it3->getName() << " ";
				  }
				  cout << endl;
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
