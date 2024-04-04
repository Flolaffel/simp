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
  params.addClassDescription("Computes the filtered sensitivities for "
                             "sensitivity (inputs: x) or density filtering (inputs: dv). "
                             "ReqInputs: filt_type, dc, r, mesh");
  params.addRequiredCoupledVar("sensitivities", "Name of the sensitivity variables.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 1;
  return params;
}

SensitivityFilterCustom::SensitivityFilterCustom(const InputParameters & parameters)
  : Filter(parameters), _n_vars(coupledComponents("sensitivities"))
{
  for (unsigned int i = 0; i < _n_vars; i++)
    _sensitivities.push_back(&writableVariable("sensitivities", i));

  if (_filter_type == FilterType::SENSITIVITY)
  {
    if (_n_vars > 1)
      mooseError("Classic sensitivity filter only requires the objective function sensitivity to "
                 "be supplied");
    _design_density_name = getParam<VariableName>("design_density");
    _design_density = &_subproblem.getStandardVariable(_tid, _design_density_name);
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
      int i = 0;
      for (auto & sensitivity : _sensitivities)
      {
        dynamic_cast<MooseVariableFE<Real> *>(sensitivity)
            ->setNodalValue(elem_data.filtered_sensitivities[i]);
        i++;
      }
    }
    else
    {
      mooseError("Element data not found for the current element id.");
    }
  }
}

void
SensitivityFilterCustom::threadJoin(const UserObject & y)
{
  const SensitivityFilterCustom & uo = static_cast<const SensitivityFilterCustom &>(y);
  _elem_data_map.insert(uo._elem_data_map.begin(), uo._elem_data_map.end());
}

void
SensitivityFilterCustom::gatherElementData()
{
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      std::vector<Real> sens_values(_n_vars);
      std::vector<Real> filt_values(_n_vars);
      int i = 0;
      for (auto & sensitivity : _sensitivities)
      {
        sens_values[i] =
            dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->getElementalValue(elem);
        i++;
      }

      if (_filter_type == FilterType::SENSITIVITY)
      {
        ElementData data = ElementData(
            sens_values,
            dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
            filt_values);
        _elem_data_map[elem_id] = data;
      }
      else if (_filter_type == FilterType::DENSITY)
      {
        ElementData data = ElementData(sens_values, 1, filt_values);
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
    temp_dc[id] = elem_data.design_density * elem_data.sensitivities[0];
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    Real filt_dc = 0;
    for (unsigned int j = 0; j < _nx * _ny; j++)
    {
      filt_dc += _H[id][j] * temp_dc[j];
    }
    filt_dc /= _Hs[id] * std::max(0.001, elem_data.design_density);
    elem_data.filtered_sensitivities[0] = filt_dc;
  }
}

void
SensitivityFilterCustom::updateSensitivitiesDensityFilter()
{
  std::vector<std::vector<Real>> temp_sens(_n_vars, std::vector<Real>(_nx * _ny));
  for (auto && [id, elem_data] : _elem_data_map)
  {
    for (unsigned int var = 0; var < temp_sens.size(); var++)
    {
      temp_sens[var][id] = elem_data.sensitivities[var] / _Hs[id];
    }
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    std::vector<Real> filt_sens(_n_vars);
    Real filt_dv = 0;
    for (unsigned int j = 0; j < _nx * _ny; j++)
    {
      for (unsigned int var = 0; var < temp_sens.size(); var++)
      {
        filt_sens[var] += _H[id][j] * temp_sens[var][j];
      }
    }
    elem_data.filtered_sensitivities = filt_sens;
  }
}

