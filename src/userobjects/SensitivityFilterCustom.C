//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SensitivityFilterCustom.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", SensitivityFilterCustom);

InputParameters
SensitivityFilterCustom::validParams()
{
  InputParameters params = Filter::validParams();
  params.addClassDescription(
      "Computes the filtered sensitivity using a radial average user object.");
  params.addRequiredCoupledVar("compliance_sensitivity",
                               "Name of the compliance_sensitivity variable.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.addCoupledVar("volume_sensitivity", "Name of the volume_sensitivity variable.");
  return params;
}

SensitivityFilterCustom::SensitivityFilterCustom(const InputParameters & parameters)
  : Filter(parameters),
    _compliance_sensitivity(&writableVariable("compliance_sensitivity")),
    _design_density_name(getParam<VariableName>("design_density")),
    _design_density(_subproblem.getStandardVariable(_tid, _design_density_name))
{
  // if (_filter_type == FilterType::SENSITIVITY)
  // {
  //   _design_density_name = getParam<VariableName>("design_density");
  //   _design_density = _subproblem.getStandardVariable(_tid, _design_density_name);
  // }
  if (_filter_type == FilterType::DENSITY)
  {
    if (!parameters.isParamSetByUser("volume_sensitivity"))
      paramError("volume_sensitivity", "No volume sensitivity for density filtering supplied.");
    else
      _volume_sensitivity = &writableVariable("volume_sensitivity");
  }
}

void
SensitivityFilterCustom::initialize()
{
  if (_filter_type != FilterType::NONE)
  {
    gatherElementData();
    Filter::prepareFilter();
    if (_filter_type == FilterType::SENSITIVITY)
      updateSensitivitiesSensitivityFilter();
    else if (_filter_type == FilterType::DENSITY)
      updateSensitivitiesDensityFilter();
  }
}

void
SensitivityFilterCustom::execute()
{
  if (_filter_type != FilterType::NONE)
  {
    // Grab the element data for each id
    auto elem_data_iter = _elem_data_map.find(_current_elem->id());

    // Check if the element data is not null
    if (elem_data_iter != _elem_data_map.end())
    {
      ElementData & elem_data = elem_data_iter->second;
      dynamic_cast<MooseVariableFE<Real> *>(_compliance_sensitivity)
          ->setNodalValue(elem_data.new_compliance_sensitivity);
      if (_filter_type == FilterType::DENSITY)
        dynamic_cast<MooseVariableFE<Real> *>(_volume_sensitivity)
            ->setNodalValue(elem_data.new_volume_sensitivity);
    }
    else
    {
      mooseError("Element data not found for the current element id.");
    }
  }
}

void
SensitivityFilterCustom::gatherElementData()
{
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      if (_filter_type == FilterType::SENSITIVITY)
      {
        ElementData data = ElementData(
            dynamic_cast<MooseVariableFE<Real> *>(_compliance_sensitivity)->getElementalValue(elem),
            _design_density.getElementalValue(elem),
            0,
            0,
            0);
        _elem_data_map[elem_id] = data;
      }
      else if (_filter_type == FilterType::DENSITY)
      {
        ElementData data = ElementData(
            dynamic_cast<MooseVariableFE<Real> *>(_compliance_sensitivity)->getElementalValue(elem),
            0,
            dynamic_cast<MooseVariableFE<Real> *>(_volume_sensitivity)->getElementalValue(elem),
            0,
            0);
        _elem_data_map[elem_id] = data;
      }
    }
}

void
SensitivityFilterCustom::updateSensitivitiesSensitivityFilter()
{
  std::vector<Real> temp_dc(_nx * _ny);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    temp_dc[id] = elem_data.design_density * elem_data.compliance_sensitivity;
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    Real filt_dc = 0;
    for (unsigned int j = 0; j < _nx * _ny; j++)
    {
      filt_dc += _H[id][j] * temp_dc[j];
    }
    filt_dc /= _Hs[id] * std::max(0.001, elem_data.design_density);
    elem_data.new_compliance_sensitivity = filt_dc;
  }
}

void
SensitivityFilterCustom::updateSensitivitiesDensityFilter()
{
  std::vector<Real> temp_dc(_nx * _ny);
  std::vector<Real> temp_dv(_nx * _ny);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    temp_dc[id] = elem_data.compliance_sensitivity / _Hs[id];
    temp_dv[id] = elem_data.volume_sensitivity / _Hs[id];
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    Real filt_dc = 0;
    Real filt_dv = 0;
    for (unsigned int j = 0; j < _nx * _ny; j++)
    {
      filt_dc += _H[id][j] * temp_dc[j];
      filt_dv += _H[id][j] * temp_dv[j];
    }
    elem_data.new_compliance_sensitivity = filt_dc;
    elem_data.new_volume_sensitivity = filt_dv;
  }
}
