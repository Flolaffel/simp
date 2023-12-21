nx = 60
ny = 20
p = 3
vol_frac = 0.5
filter_radius = 1.5
E0 = 1
Emin = 1e-9

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = ${nx}
    ymin = 0
    ymax = ${ny}
  []
  [node]
    type = ExtraNodesetGenerator
    input = MeshGenerator
    new_boundary = pull
    coord = '60 0 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '0 20 0'
  []
  [sidesets]
    type = SideSetsFromNodeSetsGenerator
    input = push
  []
[]

[AuxVariables]
  [Emin]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${Emin}
  []
  [p]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${p}
  []
  [E0]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${E0}
  []
  [Dc]
    family = MONOMIAL
    order = CONSTANT
  []
  [DV]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = 1
  []
  [rho]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
  [rhoPhys]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
[]

[AuxKernels]
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = Dc
    execute_on = TIMESTEP_END
  []
[]

[Modules/TensorMechanics/Master]
  [all]
    strain = SMALL
    add_variables = true
    incremental = false
  []
[]

[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = pull
    value = 0.0
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  []
[]

[NodalKernels]
  [pull]
    type = ConstantRate
    variable = disp_y
    boundary = push
    rate = -1
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensorPlaneStress
    youngs_modulus = E_phys
    poissons_ratio = poissons_ratio
    args = 'Emin rhoPhys p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rhoPhys ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rhoPhys'
    property_name = E_phys
  []
  [poissons_ratio]
    type = GenericConstantMaterial
    prop_names = poissons_ratio
    prop_values = 0.3
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [dc]
    type = AnalyticComplianceSensitivity
    design_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[UserObjects]
  [update]
    type = DensityUpdateCustom
    compliance_sensitivity = Dc
    volume_sensitivity = DV
    design_density = rho
    physical_density = rhoPhys
    filter_type = none
    volume_fraction = ${vol_frac}
    execute_on = TIMESTEP_END
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to Dc (aux variable)
  [calc_sense]
    type = SensitivityFilterCustom
    filter_type = sensitivity
    compliance_sensitivity = Dc
    design_density = rho
    radius = ${filter_radius}
    mesh_generator = MeshGenerator
    execute_on = TIMESTEP_END
    force_postaux = true
  []
  #[opt_conv]
  #  type = Terminator
  #  expression = 'stop_vol < 1e-5 & SE_stop < 1e-5'
  #  execute_on = TIMESTEP_END
  #[]
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-8
  dt = 1.0
  num_steps = 94
[]

[Outputs]
  exodus = true
[]

[Postprocessors]
  [total_vol]
    type = ElementIntegralVariablePostprocessor
    variable = rho
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [vol_change]
    type = ChangeOverTimePostprocessor
    postprocessor = total_vol
    change_with_respect_to_initial = false
    execute_on = 'initial timestep_end'
  []
  [n_el]
    type = ConstantPostprocessor
    value = '${fparse nx*ny}'
  []
  [stop_vol]
    type = ParsedPostprocessor
    function = 'abs(vol_change/n_el)'
    pp_names = 'vol_change n_el'
  []
  [SE]
    type = ElementIntegralMaterialProperty
    mat_prop = strain_energy_density
  []
  [SE_stop]
    type = ChangeOverTimePostprocessor
    postprocessor = SE
    change_with_respect_to_initial = false
    compute_relative_change = true
    take_absolute_value = true
    execute_on = 'initial timestep_end'
  []
[]

#[Debug]
#  show_material_props = true
#  show_execution_order = ALWAYS
#[]
