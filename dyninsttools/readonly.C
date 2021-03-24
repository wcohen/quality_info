/*
 * readonly.C
 *
 * readonly is an attempt to get some information about which variables
 * are readonly at various places in the binary.
 *
 * This code was derived from the Ben Woodard's dwqual/linemap.C
 * the dyninst dataflowAPI documentation liveness Analysis example.
 */

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

	  /* Track variables for the function (and any inlined functions it contains)*/
	  search_and_track_variables(session, register_loclist, func_sym);

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
