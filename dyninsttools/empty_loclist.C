/*
 *  Derived from Josh Stone's  dyninst-nontrivial-param.cc in https://github.com/cuviper/nontrivial-param.git
 *
 *  This code is a simple experiment to see if there are empty location lists for functions.
 *  One significant improvement is that code also inspect inlined functions.
 */
#include <dyninst/Symtab.h>
#include <dyninst/Function.h>

using namespace std;
using namespace Dyninst::SymtabAPI;

static bool
has_loclist (localVar param)
{
  return !(param.getLocationLists().empty());
}

static void
process_function (FunctionBase *function)
{
  vector<localVar*> parameters;
  if (!function->getParams (parameters) || parameters.empty())
    return;

  bool printed_function_name = false;
  for (auto parameter : parameters)
    {
    if (has_loclist (*parameter))
        continue;

      if (!printed_function_name)
        {
	  cout << parameter->getFileName() << ": In function ‘"
	       << function->getName() << "’:" << endl;
          printed_function_name = true;
        }

      cout << parameter->getFileName() << ":" << parameter->getLineNum()
           << ": note: parameter ‘" << parameter->getName()
           << "’ has no location list" << endl;
    }
}

static void
expand_inlined_process(FunctionBase *function)
{
    process_function (function);
    const InlineCollection& inlined_functions = function->getInlines();
    // Check to see if there are inlined functions nested inside
    for (auto f: inlined_functions) {
	    expand_inlined_process(f);
    }
}

int
main (int argc, char *argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;

  Symtab *symtab;
  vector<Function*> functions;
  if (!Symtab::openFile (symtab, argv[1]) ||
      !symtab->getAllFunctions (functions))
    {
      auto err = Symtab::getLastSymtabError();
      clog << Symtab::printError (err) << endl;
      return EXIT_FAILURE;
    }

  for (auto function : functions)
    expand_inlined_process (function);

  return EXIT_SUCCESS;
}
