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
  InputParameters params = FilterBase::validParams();
  params.addClassDescription("Computes the filtered sensitivities for "
                             "sensitivity filtering, density filtering or Heaviside projection.");
  params.addRequiredCoupledVar("sensitivities", "Name of the sensitivity variables.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.addParam<VariableName>("filtered_density", "Filtered density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 1;
  return params;
}

SensitivityFilterCustom::SensitivityFilterCustom(const InputParameters & parameters)
  : FilterBase(parameters), _n_vars(coupledComponents("sensitivities"))
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
  else if (_filter_type == FilterType::HEAVISIDE)
  {
    _filtered_density_name = getParam<VariableName>("filtered_density");
    _filtered_density = &_subproblem.getStandardVariable(_tid, _filtered_density_name);
  }
}

void
SensitivityFilterCustom::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize SensitvityFilterCustom");
  if (_filter_type != FilterType::NONE)
  {
    gatherElementData();

    if (_app.n_processors() > 1)
    {
      for (unsigned int var = 0; var < _n_vars; var++)
      {
        _communicator.allgather(_sensitivities_vec[var], false);
        std::sort(_sensitivities_vec[var].begin(), _sensitivities_vec[var].end());
      }
      if (_filter_type == FilterType::SENSITIVITY)
      {
        _communicator.allgather(_design_density_vec, false);
        std::sort(_design_density_vec.begin(), _design_density_vec.end());
      }
      if (_filter_type == FilterType::HEAVISIDE)
      {
        _communicator.allgather(_filtered_density_vec, false);
        std::sort(_filtered_density_vec.begin(), _filtered_density_vec.end());
      }
    }

    if (_filter_type == FilterType::SENSITIVITY)
      updateSensitivitiesSensitivityFilter();
    else if (_filter_type == FilterType::DENSITY)
      updateSensitivitiesDensityFilter();
    else if (_filter_type == FilterType::HEAVISIDE)
      updateSensitivitiesHeaviside();
  }
}

void
SensitivityFilterCustom::execute()
{
  if (_filter_type != FilterType::NONE)
  {
    // Grab the element data for each id
    auto elem_data_iter = _filtered_sensitivities_map.find(_current_elem->id());

    // Check if the element data is not null
    mooseAssert(elem_data_iter != _filtered_sensitivities_map.end(),
                "Element data not found for the current element id.");

    auto & filtered_sens = elem_data_iter->second;
    int i = 0;
    for (auto & sensitivity : _sensitivities)
    {
      dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->setNodalValue(filtered_sens[i]);
      i++;
    }
  }
}

void
SensitivityFilterCustom::threadJoin(const UserObject & y)
{
  TIME_SECTION("threadJoin", 3, "Joining Threads");
  const SensitivityFilterCustom & uo = static_cast<const SensitivityFilterCustom &>(y);
  _filtered_sensitivities_map.insert(uo._filtered_sensitivities_map.begin(),
                                     uo._filtered_sensitivities_map.end());
}

void
SensitivityFilterCustom::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _filtered_sensitivities_map.clear();
  _sensitivities_vec.clear();
  _design_density_vec.clear();
  _filtered_density_vec.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      std::vector<Real> sens_values(_n_vars);
      int i = 0;
      for (auto & sensitivity : _sensitivities)
      {
        sens_values[i] =
            dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->getElementalValue(elem);
        i++;
      }
      _sensitivities_vec.resize(_n_vars);
      for (unsigned int var = 0; var < _n_vars; var++)
      {
        _sensitivities_vec[var].emplace_back(elem_id, sens_values[var]);
      }

      if (_filter_type == FilterType::SENSITIVITY)
      {
        _design_density_vec.emplace_back(
            elem_id,
            dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem));
      }
      if (_filter_type == FilterType::HEAVISIDE)
      {
        _filtered_density_vec.emplace_back(
            elem_id,
            dynamic_cast<MooseVariableFE<Real> *>(_filtered_density)->getElementalValue(elem));
      }
    }
}

void
SensitivityFilterCustom::updateSensitivitiesSensitivityFilter()
{
  TIME_SECTION("updateSensitivitiesSensitivityFilter", 3, "Filtering Objective Sensitivity");
  RealEigenVector sens(_n_el), dens(_n_el);
  for (unsigned int i = 0; i < _n_el; i++)
  {
    sens(i) = _sensitivities_vec[0][i].second;
    dens(i) = _design_density_vec[i].second;
  }

  RealEigenVector min = 0.001 * RealEigenVector::Ones(_n_el);
  RealEigenVector temp = min.cwiseMax(dens);
  RealEigenVector filt_sens =
      (_H * (dens.array() * sens.array()).matrix()).array() / _Hs.array() / temp.array();

  for (unsigned int i = 0; i < _n_el; i++)
  {
    _filtered_sensitivities_map[i] = {filt_sens(i)};
  }
}

void
SensitivityFilterCustom::updateSensitivitiesDensityFilter()
{
  TIME_SECTION("updateSensitivitiesDensityFilter", 3, "Updating Sensitivities");
  RealEigenMatrix temp_sens(_n_el, _n_vars);
  for (unsigned int i = 0; i < _n_el; i++)
  {
    for (unsigned int var = 0; var < temp_sens.cols(); var++)
    {
      temp_sens(i, var) = _sensitivities_vec[var][i].second / _Hs(i);
    }
  }

  RealEigenMatrix filt_sens = _H * temp_sens;

  for (unsigned int i = 0; i < _n_el; i++)
  {
    std::vector<Real> assign(_n_vars);
    RealEigenVector::Map(&assign[0], _n_vars) = filt_sens.row(i);
    _filtered_sensitivities_map[i] = assign;
  }
}

void
SensitivityFilterCustom::updateSensitivitiesHeaviside()
{
  TIME_SECTION("updateSensitivitiesHeaviside", 3, "Updating Sensitivities");
  RealEigenVector dx(_n_el);
  RealEigenMatrix sens(_n_el, _n_vars);
  for (unsigned int i = 0; i < _n_el; i++)
  {
    dx(i) = (_beta * std::pow(1 / std::cosh(_beta * (_filtered_density_vec[i].second - _eta)), 2)) /
            (std::tanh(_beta * _eta) + std::tanh(_beta * (1 - _eta)));
    for (unsigned int var = 0; var < _n_vars; var++)
    {
      sens(i, var) = _sensitivities_vec[var][i].second;
    }
  }

  RealEigenMatrix filt_sens(_n_el, _n_vars);
  for (unsigned int var = 0; var < _n_vars; var++)
  {
    filt_sens.col(var) = _H * (sens.col(var).array() * dx.array() / _Hs.array()).matrix();
  }

  for (unsigned int i = 0; i < _n_el; i++)
  {
    std::vector<Real> assign(_n_vars);
    RealEigenVector::Map(&assign[0], _n_vars) = filt_sens.row(i);
    _filtered_sensitivities_map[i] = assign;
  }
}
