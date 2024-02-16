//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "TopologyOptimizationDesignResponse.h"
// #include "MooseError.h"
// #include "NonlinearSystemBase.h"
// #include "NodalBCBase.h"
// #include "MooseVariableScalar.h"
// #include "vector.h"

// #include "libmesh/petsc_matrix.h"
// #include "libmesh/sparse_matrix.h"

// #include <algorithm>

InputParameters
TopologyOptimizationDesignResponse::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Base class for topology optimzation design responses that deliver absolute value for whole "
      "domain and sensitivity with respect to design variables");
  params.addParam<Real>("limit", 1, "Limit"); /*upper limit for now*/
  params.addRequiredParam<AuxVariableName>("value", "Name of the value variable.");
  params.addRequiredCoupledVar("sensitivity", "Name of the sensitivity variable.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.addParam<VariableName>("physical_density", "Physical density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 0;
  return params;
}

TopologyOptimizationDesignResponse::TopologyOptimizationDesignResponse(
    const InputParameters & parameters)
  : ElementUserObject(parameters),
    _limit(getParam<Real>("limit")),
    _value(&_subproblem.getScalarVariable(_tid, parameters.get<AuxVariableName>("value"))),
    _sensitivity(&writableVariable("sensitivity")),
    _design_density_name(getParam<VariableName>("design_density")),
    _design_density(&_subproblem.getStandardVariable(_tid, _design_density_name)),
    _physical_density_name(getParam<VariableName>("physical_density")),
    _physical_density(&_subproblem.getStandardVariable(_tid, _physical_density_name))
{
}
