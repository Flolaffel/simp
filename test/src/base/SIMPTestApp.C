//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html
#include "SIMPTestApp.h"
#include "SIMPApp.h"
#include "Moose.h"
#include "AppFactory.h"
#include "MooseSyntax.h"

InputParameters
SIMPTestApp::validParams()
{
  InputParameters params = SIMPApp::validParams();
  params.set<bool>("use_legacy_material_output") = false;
  return params;
}

SIMPTestApp::SIMPTestApp(InputParameters parameters) : MooseApp(parameters)
{
  SIMPTestApp::registerAll(
      _factory, _action_factory, _syntax, getParam<bool>("allow_test_objects"));
}

SIMPTestApp::~SIMPTestApp() {}

void
SIMPTestApp::registerAll(Factory & f, ActionFactory & af, Syntax & s, bool use_test_objs)
{
  SIMPApp::registerAll(f, af, s);
  if (use_test_objs)
  {
    Registry::registerObjectsTo(f, {"SIMPTestApp"});
    Registry::registerActionsTo(af, {"SIMPTestApp"});
  }
}

void
SIMPTestApp::registerApps()
{
  registerApp(SIMPApp);
  registerApp(SIMPTestApp);
}

/***************************************************************************************************
 *********************** Dynamic Library Entry Points - DO NOT MODIFY ******************************
 **************************************************************************************************/
// External entry point for dynamic application loading
extern "C" void
SIMPTestApp__registerAll(Factory & f, ActionFactory & af, Syntax & s)
{
  SIMPTestApp::registerAll(f, af, s);
}
extern "C" void
SIMPTestApp__registerApps()
{
  SIMPTestApp::registerApps();
}
