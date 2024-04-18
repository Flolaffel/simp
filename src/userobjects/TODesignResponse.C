//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "TODesignResponse.h"
#include "MooseError.h"

InputParameters
TODesignResponse::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Base class for topology optimzation design responses that deliver absolute value for whole "
      "domain and sensitivity with respect to design variables");
  params.addRequiredParam<MooseEnum>("usage", TODesignResponse::getUsageEnum(), "The usage");
  params.addParam<Real>("limit", "Limit"); /*upper limit for now*/
  params.addRequiredParam<AuxVariableName>("value", "Name of the value variable.");
  params.addRequiredCoupledVar("sensitivity", "Name of the sensitivity variable.");
  params.addParam<VariableName>("physical_density", "Physical density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 0;
  return params;
}

TODesignResponse::TODesignResponse(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _usage(getParam<MooseEnum>("usage").getEnum<Usage>()),
    _is_objective(_usage == Usage::OBJECTIVE),
    _is_constraint(_usage == Usage::CONSTRAINT),
    _value(&_subproblem.getScalarVariable(_tid, parameters.get<AuxVariableName>("value"))),
    _sensitivity(&writableVariable("sensitivity")),
    _physical_density_name(getParam<VariableName>("physical_density")),
    _physical_density(&_subproblem.getStandardVariable(_tid, _physical_density_name))
{
  if (_is_constraint && isParamValid("limit"))
    _limit = getParam<Real>("limit");
  if (_is_constraint && !isParamValid("limit"))
    mooseError("Limit needed when usage is set to constraint");
  else if (_is_objective && isParamValid("limit"))
    mooseError("No limit needed when usage is set to objective function");
}

MooseEnum
TODesignResponse::getUsageEnum()
{
  auto usage = MooseEnum("objective constraint", "");
  return usage;
}
