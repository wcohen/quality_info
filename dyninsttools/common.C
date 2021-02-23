/*
 * common.C
 *
 * common code used by the various analysis tools
 * -handling of filters
 *
 */

#include <argp.h>
#include <map>
#include <regex>
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

string noreason("");

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
      arguments->dbg_filter = true;
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
  session.dbg_filter = false;

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


void filter_range(session_info &session, string filter, Address start, Address end)
{
	if (session.dbg_filter) {
		cout << "filter_range: " << filter << " [" << hex << start << "," << hex << end << ")" << endl;
	}
	session.interest_reason.add(make_pair(interval<Address>::right_open(start, end), filter));
	return;
}

void filter_find_funcs (session_info &session, string filter, string func)
{
	if (session.dbg_filter) {
		cout << "filter_find_funcs: " << filter << endl;
	}
	vector<SymtabAPI::Function *> functions_of_interest;
	bool matches = session.syms->findFunctionsByName(functions_of_interest,
							 func, anyName,
							 true, true);

	/* Right now mark the entire regular function */
	for (auto f: functions_of_interest) {
		/* FIXME need to find location of inlined functions */
		/* FIXME Make sure that the address ranges are reasonable */
		Address start = f->getOffset();
		Address end = start+f->getSize();
		filter_range(session, filter, start, end);
	}
}

void filter_find_func_calls(session_info &session, string filter, string func)
{
	if (session.dbg_filter) {
		cout << "filter_func_calls: " << filter << endl;
	}
	vector<SymtabAPI::Function *> functions_of_interest;
	bool matches = session.syms->findFunctionsByName(functions_of_interest,
							 func, anyName, true, true);

	/* Just mark the first instruction of the function(s). */
	for (auto f: functions_of_interest) {
		Address start = f->getOffset();
		filter_range(session, filter, start, start);
	}
}

void filter_find_statement(session_info &session, string filter, string file, int line)
{
	if (session.dbg_filter) {
		cout << "filter_statement: " << filter << endl;
	}
	// FIXME doesn't handle wildcards for file or line numbers (or any function) filter.
	std::vector<AddressRange> ranges;
	bool found_lines = session.syms->getAddressRanges(ranges, file, line);

	if (!found_lines) return;
	for (auto f: ranges) {
		filter_range(session, filter, f.first , f.second);
	}
}

void dump_loclist(localVar *var)
{
	cout << var->getName() << " ";
	vector<VariableLocation> &lvlocs=var->getLocationLists();
	for(auto k: lvlocs) {
		cout << k.stClass << " " << k.refClass << " "  << k.mr_reg
		     << " " << k.frameOffset << " " << k.frameOffsetAbs
		     << hex << " " << k.lowPC << " " << k.hiPC << endl;
	}
}

void filter_search_inline(session_info &session, string filter, string func, SymtabAPI::FunctionBase *f)
{
	std::pair<std::string, Dyninst::Offset> location = dynamic_cast<Dyninst::SymtabAPI::InlinedFunction*>(f)->getCallsite();

	if (session.dbg_filter) {
		cout << "inlined: " << f->getName()
		     << " offset: " << hex << f->getOffset() << " "
		     << location.first << ":" << dec << location.second << endl;

		vector <localVar *> lvars;
		f->getParams(lvars);
		for(auto j: lvars) {
			dump_loclist(j);
		}
		vector <localVar *> pvars;
		f->getLocalVariables(pvars);
		for(auto j: pvars) {
			dump_loclist(j);
		}
	}
	// FIXME Add the inlined to list of points of interest
	// Recursively get inlined functions in the inlined function
	const InlineCollection& inlined_functions = f->getInlines();
	for (auto fff: inlined_functions) {
		if (session.dbg_filter) {
			cout << f->getName() << "->" << fff->getName() << endl;
		}
		filter_search_inline(session, filter, fff->getName(), fff);
	}
}


void filter_find_func_inline(session_info &session, string filter, string func)
{
	if (session.dbg_filter) {
		cout << "filter_inline_func: " << filter << endl;
	}
	// FIXME Need to iterate through all the functions and look for inlined functions call sites.
	vector<SymtabAPI::Function *> functions_of_interest;
	bool matches = session.syms->findFunctionsByName(functions_of_interest,
							 "*", anyName,
							 true, true);

	// Process inline functioned for each of the actual functions
	for (auto ff: functions_of_interest) {
		// Get list of inline functions in function
		const InlineCollection& inlined_functions = ff->getInlines();
		// Check to see if there are inlined funcitons inside this
		for (auto fff: inlined_functions) {
			filter_search_inline(session, filter, fff->getName(), fff);
		}
	}
}

static const std::regex probe_call("process\\(\"([^\"]+)\"\\)\\.function\\(\"([^\"]+)\"\\)\\.call");
static const std::regex probe_return("process\\(\"([^\"]+)\"\\)\\.function\\(\"([^\"]+)\"\\)\\.return");
static const std::regex probe_inline("process\\(\"([^\"]+)\"\\)\\.function\\(\"([^\"]+)\"\\)\\.inline");
static const std::regex probe_statement("process\\(\"([^\"]+)\"\\)\\.statement\\(\"([^@]+)@([^:]+):([^\"]+)\"\\)");
static const std::regex probe_statement_nearest("process\\(\"([^\"]+)\"\\)\\.statement\\(\"([^@]+)@([^:]+):([^\"]+)\"\\).nearest");
static const std::regex probe_function("process\\(\"([^\"]+)\"\\)\\.function\\(\"([^\"]+)\"\\)");

exit_codes process_filters(session_info &session)
{
	/* Go through each of the filters and see which addresses match */
	for (auto filter : session.filters) {
		std::smatch matches;
		if (session.dbg_filter) {
			cout << "filter: " << filter << endl;
		}
		// FIXME Handles these other kinds of probe locations:
		// function entry
		if (regex_search(filter, matches, probe_call)){
			filter_find_func_calls(session, filter, matches[2]);
			continue;
		}
		// function return
		if (regex_search(filter, matches, probe_return)){
			filter_find_func_calls(session, filter, matches[2]);
			continue;
		}
		// line numbers
		if (regex_search(filter, matches, probe_statement)){
			// FIXME doesn't handle wildcards for line numbers
			int line = stoi(matches[4]);
			filter_find_statement(session, filter, matches[3], line);
			continue;
		}
		// inlined function entry
		if (regex_search(filter, matches, probe_inline)){
			filter_find_func_inline(session, filter, matches[2]);
			continue;
		}
		// either function entry or inlined
		if (regex_search(filter, matches, probe_function)){
			filter_find_funcs(session, filter, matches[2]);
			continue;
		}
		// assume filter argument is just a function name
		filter_find_funcs(session, filter, filter);
	}
	// If no filters in options, just make defaults ones.
	// One filter for each function using wildcard ("*").
	if (session.filters.size() == 0) {
		filter_find_funcs(session, "default",  "*");
	}
	return(EXIT_OK);
}

