nx = 60
ny = 20
p = 3
vol_frac = 0.5
filter_radius = 1.5
E0 = 1
Emin = 1e-9
nu = 0.3

[GlobalParams]
  displacements = 'disp_x disp_y'
  design_density = rho
  physical_density = rhoPhys
  volume_fraction = ${vol_frac}
  radius = ${filter_radius}
  E0 = ${E0}
  Emin = ${Emin}
  p = ${p}
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
    coord = '${fparse nx} 0 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '0 ${fparse ny} 0'
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
  []
  [KS]
    family = SCALAR
    order = FIRST
  []
  [dKS]
    family = MONOMIAL
    order = CONSTANT
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
    generate_output = "stress_xx stress_yy stress_xy vonmises_stress"
  []
[]

[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = pull
    value = 0.0
    matrix_tags = system
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
    matrix_tags = system
  []
[]

[NodalKernels]
  [pull]
    type = ConstantRate
    variable = disp_y
    boundary = push
    rate = -1
    matrix_tags = system
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
    prop_values = ${nu}
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [dc]
    type = AnalyticComplianceSensitivity
    design_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
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
    update_scheme = MMA
    objective_function_sensitivity = dc
    constraint_values = 'V KS'
    constraint_sensitivities = 'dV dKS'
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
    filter_type = density
    mesh_generator = MeshGenerator
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to Dc aux variable
  [filt_sens]
    type = SensitivityFilterCustom
    filter_type = density
    sensitivities = 'dc dV dKS'
    mesh_generator = MeshGenerator
  []
  [stress_sens]
    type = StressResponse
    usage = constraint
    limit = 1
    value = KS
    sensitivity = dKS
    stresses = 'vonmises_stress stress_xx stress_xy stress_yy'
    poissons_ratio = ${nu}
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
  num_steps = 100
[]

[Outputs]
  exodus = true
[]

[Debug]
  # show_material_props = true
  # show_execution_order = ALWAYS
[]
