//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "TestElementUserObject.h"
//#include "MooseError.h"
#include "Output.h"
#include <algorithm>

registerMooseObject("OptimizationApp", TestElementUserObject);

InputParameters
TestElementUserObject::validParams()
{
  InputParameters params = ElementUserObject::validParams();

  return params;
}

TestElementUserObject::TestElementUserObject(const InputParameters & parameters)
  : ElementUserObject(parameters)
{
}

void
TestElementUserObject::initialize()
{
  _console << "initialize" << "\n" << std::flush;
}

void
TestElementUserObject::execute()
{
  _console << "execute" << "\n" << std::flush;
}

void
TestElementUserObject::finalize()
{
  _console << "finalize" << "\n" << std::flush;
}