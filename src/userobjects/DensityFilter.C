//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityFilter.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", DensityFilter);

InputParameters
DensityFilter::validParams()
{
  InputParameters params = FilterBase::validParams();
  params.addClassDescription("Density Filter for SIMP topology optimization");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredCoupledVar("physical_density", "Physical density variable name.");
  params.set<MooseEnum>("filter_type") = "density";
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 3;
  return params;
}

DensityFilter::DensityFilter(const InputParameters & parameters)
  : FilterBase(parameters),
    _design_density(&writableVariable("design_density")),
    _physical_density(&writableVariable("physical_density"))
{
}

void
DensityFilter::initialize()
{
  TIME_SECTION("initialize", 2, "Preparing DensityFilter");
  gatherElementData();
  if (_app.n_processors() > 1)
  {
    _communicator.allgather(_design_density_vec, false);
    mooseAssert(_design_density_vec.size() == _n_el, "MPI gathering failed.");
    std::sort(_design_density_vec.begin(), _design_density_vec.end());
  }
  densityFilter();
}

void
DensityFilter::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _filtered_density_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _filtered_density_map.end(),
              "Element data not found for the current element id.");

  Real & filtered_density = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_physical_density)->setNodalValue(filtered_density);
}

void
DensityFilter::threadJoin(const UserObject & y)
{
  TIME_SECTION("threadJoin", 3, "Join VolumeResponse Threads");
  const DensityFilter & uo = static_cast<const DensityFilter &>(y);
  _filtered_density_map.insert(uo._filtered_density_map.begin(), uo._filtered_density_map.end());
}

void
DensityFilter::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _filtered_density_map.clear();
  _design_density_vec.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      _design_density_vec.emplace_back(
          elem->id(),
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem));
    }
}

void
DensityFilter::densityFilter()
{
  TIME_SECTION("densityFilter", 3, "Filtering Density");
  RealEigenVector density(_n_el);
  for (unsigned int i = 0; i < _n_el; i++)
  {
    density(i) = _design_density_vec[i].second;
  }

  RealEigenVector filtered = (_H * density).array() / _Hs.array();

  for (unsigned int i = 0; i < _n_el; i++)
  {
    _filtered_density_map[i] = filtered(i);
  }
}
