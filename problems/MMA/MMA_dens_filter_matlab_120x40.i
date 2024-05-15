nx = 120
ny = 40
p = 3
vol_frac = 0.5
filter_radius = 1.5
E0 = 1
Emin = 1e-9

[GlobalParams]
  displacements = 'disp_x disp_y'
  design_density = rho
  physical_density = rhoPhys
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = 60
    ymin = 0
    ymax = 20
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
  [dc]
    family = MONOMIAL
    order = CONSTANT
  []
  [V]
    family = SCALAR
    order = FIRST
  []
  [dV]
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
  [rho_old1]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
  [rho_old2]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${vol_frac}
  []
  [low]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = 1
  []
  [upp]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = 1
  []
[]

[AuxKernels]
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = dc
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
    physical_density = rhoPhys
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
    type = DensityUpdateMMA
    objective_function_sensitivity = dc
    constraint_values = 'V'
    constraint_sensitivities = dV
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to dc aux variable
  [calc_sense]
    type = SensitivityFilterCustom
    sensitivities = 'dc dV'
    radius = ${filter_radius}
    mesh_generator = MeshGenerator
    filter_type = density
  []
  [filter]
    type = DensityFilter
    design_density = rho
    physical_density = rhoPhys
    radius = ${filter_radius}
    mesh_generator = MeshGenerator
  []
  [vol_sens]
    type = VolumeResponse
    usage = constraint
    limit = ${vol_frac}
    value = V
    sensitivity = dV
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-8
  dt = 1.0
  num_steps = 70
[]

[Outputs]
  exodus = true
  csv = true
[]

[Postprocessors]
  [total_vol]
    type = ElementIntegralVariablePostprocessor
    variable = rho
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Debug]
 # show_material_props = true
 # show_execution_order = ALWAYS
[]
