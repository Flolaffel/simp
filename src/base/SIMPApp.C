#include "SIMPApp.h"
#include "Moose.h"
#include "AppFactory.h"
#include "ModulesApp.h"
#include "MooseSyntax.h"

InputParameters
SIMPApp::validParams()
{
  InputParameters params = MooseApp::validParams();
  params.set<bool>("use_legacy_material_output") = false;
  params.set<bool>("use_legacy_initial_residual_evaluation_behavior") = false;
  return params;
}

SIMPApp::SIMPApp(InputParameters parameters) : MooseApp(parameters)
{
  SIMPApp::registerAll(_factory, _action_factory, _syntax);
}

SIMPApp::~SIMPApp() {}

void 
SIMPApp::registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  ModulesApp::registerAllObjects<SIMPApp>(f, af, s);
  Registry::registerObjectsTo(f, {"SIMPApp"});
  Registry::registerActionsTo(af, {"SIMPApp"});

  /* register custom execute flags, action syntax, etc. here */
}

void
SIMPApp::registerApps()
{
  registerApp(SIMPApp);
}

/***************************************************************************************************
 *********************** Dynamic Library Entry Points - DO NOT MODIFY ******************************
 **************************************************************************************************/
extern "C" void
SIMPApp__registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  SIMPApp::registerAll(f, af, s);
}
extern "C" void
SIMPApp__registerApps()
{
  SIMPApp::registerApps();
}
