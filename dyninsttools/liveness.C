/*
 * liveness.C
 *
 * liveness is an attempt to get some information about which register
 * are live at various places in the binary.
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
	  LivenessAnalyzer la(f->obj()->cs()->getAddressWidth());

	  printf("# %s\n", f->name().c_str());
	  la.analyze(f);

	  printf ("# %s,%s,%s\n", "address", "in", "out");
	  // go through each block
	  for (auto b = f->blocks().begin(); b != f->blocks().end(); ++b) {
		  Block *bb = *b;
		  Block::Insns insns;

		  printf("# bb start %p end %p\n", bb->start(),  bb->last());

		  bb->getInsns(insns);
		  // go through each instruction in the block
		  for (auto insn_iter : insns) {
			  Instruction curInsn = insn_iter.second;
			  Address curAddr = insn_iter.first;

			  // construct a liveness query location
			  InsnLoc i(bb, curAddr, curInsn);
			  Location loc(f, i);
			  
			  // Query live registers at instruction entry
			  bitArray liveEntry;
			  if (!la.query(loc, LivenessAnalyzer::Before, liveEntry)) {
				  printf("Cannot look up live registers at instruction entry\n");
			  }

			  // Query live registers at instruction exit
			  bitArray liveExit;
			  if (!la.query(loc, LivenessAnalyzer::After, liveExit)) {
				  printf("Cannot look up live registers at instruction exit\n");
			  }

			  printf ("%p,%d,%d\n", curAddr, liveEntry.count(), liveExit.count());
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
