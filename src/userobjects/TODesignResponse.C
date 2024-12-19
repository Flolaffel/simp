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

MooseEnum
TODesignResponse::getUsageEnum()
{
  auto usage = MooseEnum("objective constraint", "");
  return usage;
}

MooseEnum
TODesignResponse::getInequalityTypeEnum()
{
  auto type = MooseEnum("le ge", "le");

  type.addDocumentation("le", "Inequality constraint, less equal.");
  type.addDocumentation("ge", "Inequality constraint, greater equal.");
  return type;
}

InputParameters
TODesignResponse::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Base class for topology optimzation design responses that deliver absolute value for whole "
      "domain and sensitivity with respect to design variables");
  params.addRequiredParam<MooseEnum>("usage", TODesignResponse::getUsageEnum(), "The usage");
  params.addParam<MooseEnum>("inequality_type",
                             TODesignResponse::getInequalityTypeEnum(),
                             "Whether the limit should be used as upper limit (less equal, le) or "
                             "lower limit (greater equal, ge).");
  params.addParam<Real>("limit", "Limit"); /*upper limit for now*/
  params.addRequiredParam<AuxVariableName>("value", "Name of the value variable.");
  params.addRequiredCoupledVar("sensitivity", "Name of the sensitivity variable.");
  params.addParam<VariableName>("physical_density", "Physical density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 0;
  params.addParam<bool>("scaling", false, "Whether to scale the constraint.");
  return params;
}

TODesignResponse::TODesignResponse(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _n_el(_mesh.getMesh().n_elem()),
    _usage(getParam<MooseEnum>("usage").getEnum<Usage>()),
    _inequality_type(getParam<MooseEnum>("inequality_type").getEnum<InequalityType>()),
    _is_objective(_usage == Usage::OBJECTIVE),
    _is_constraint(_usage == Usage::CONSTRAINT),
    _scalar_value(&_subproblem.getScalarVariable(_tid, parameters.get<AuxVariableName>("value"))),
    _sensitivity(&writableVariable("sensitivity")),
    _physical_density_name(getParam<VariableName>("physical_density")),
    _physical_density(&_subproblem.getStandardVariable(_tid, _physical_density_name)),
    _scaling(getParam<bool>("scaling"))
{
  if (_is_constraint && isParamValid("limit"))
  {
    _limit = getParam<Real>("limit");
    _scaled_limit = _limit;
  }
  if (_is_constraint && !isParamValid("limit"))
    mooseError("Limit needed when usage is set to constraint");
  else if (_is_objective && isParamValid("limit"))
    mooseError("No limit needed when usage is set to objective function");

  if (_inequality_type == InequalityType::LE)
    _con_sign = 1;
  if (_inequality_type == InequalityType::GE)
    _con_sign = -1;
}
