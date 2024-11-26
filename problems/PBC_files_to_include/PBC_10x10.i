# - EVC: zu großes Delta führt zu falschem Ergebnis
# - LNC mit NL und N1/2 in Kombination mit EVC für NL und NR geht nicht, da alle NL Knoten auf einen Wert gesetzt werden
[Constraints]
  [lr1_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 3'
    weights = '-1 1 1'
    secondary_node_ids = 21
    penalty = 1e9
    formulation = kinematic
  []
  [lr1_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 3'
    weights = '-1 1 1'
    secondary_node_ids = 21
    penalty = 1e9
    formulation = kinematic
  []
  [lr2_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 23'
    weights = '-1 1 1'
    secondary_node_ids = 32
    penalty = 1e9
    formulation = kinematic
  []
  [lr2_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 23'
    weights = '-1 1 1'
    secondary_node_ids = 32
    penalty = 1e9
    formulation = kinematic
  []
  [lr3_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 34'
    weights = '-1 1 1'
    secondary_node_ids = 43
    penalty = 1e9
    formulation = kinematic
  []
  [lr3_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 34'
    weights = '-1 1 1'
    secondary_node_ids = 43
    penalty = 1e9
    formulation = kinematic
  []
  [lr4_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 45'
    weights = '-1 1 1'
    secondary_node_ids = 54
    penalty = 1e9
    formulation = kinematic
  []
  [lr4_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 45'
    weights = '-1 1 1'
    secondary_node_ids = 54
    penalty = 1e9
    formulation = kinematic
  []
  [lr5_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 56'
    weights = '-1 1 1'
    secondary_node_ids = 65
    penalty = 1e9
    formulation = kinematic
  []
  [lr5_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 56'
    weights = '-1 1 1'
    secondary_node_ids = 65
    penalty = 1e9
    formulation = kinematic
  []
  [lr6_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 67'
    weights = '-1 1 1'
    secondary_node_ids = 76
    penalty = 1e9
    formulation = kinematic
  []
  [lr6_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 67'
    weights = '-1 1 1'
    secondary_node_ids = 76
    penalty = 1e9
    formulation = kinematic
  []
  [lr7_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 78'
    weights = '-1 1 1'
    secondary_node_ids = 87
    penalty = 1e9
    formulation = kinematic
  []
  [lr7_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 78'
    weights = '-1 1 1'
    secondary_node_ids = 87
    penalty = 1e9
    formulation = kinematic
  []
  [lr8_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 89'
    weights = '-1 1 1'
    secondary_node_ids = 98
    penalty = 1e9
    formulation = kinematic
  []
  [lr8_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 89'
    weights = '-1 1 1'
    secondary_node_ids = 98
    penalty = 1e9
    formulation = kinematic
  []
  [lr9_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 100'
    weights = '-1 1 1'
    secondary_node_ids = 109
    penalty = 1e9
    formulation = kinematic
  []
  [lr9_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 100'
    weights = '-1 1 1'
    secondary_node_ids = 109
    penalty = 1e9
    formulation = kinematic
  []
  [lr10_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 20 111'
    weights = '-1 1 1'
    secondary_node_ids = 120
    penalty = 1e9
    formulation = kinematic
  []
  [lr10_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 20 111'
    weights = '-1 1 1'
    secondary_node_ids = 120
    penalty = 1e9
  []
  [tb1_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 1'
    weights = '-1 1 1'
    secondary_node_ids = 110
    penalty = 1e9
    formulation = kinematic
  []
  [tb1_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 1'
    weights = '-1 1 1'
    secondary_node_ids = 110
    penalty = 1e9
    formulation = kinematic
  []
  [tb2_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 4'
    weights = '-1 1 1'
    secondary_node_ids = 112
    penalty = 1e9
    formulation = kinematic
  []
  [tb2_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 4'
    weights = '-1 1 1'
    secondary_node_ids = 112
    penalty = 1e9
    formulation = kinematic
  []
  [tb3_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 6'
    weights = '-1 1 1'
    secondary_node_ids = 113
    penalty = 1e9
    formulation = kinematic
  []
  [tb3_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 6'
    weights = '-1 1 1'
    secondary_node_ids = 113
    penalty = 1e9
    formulation = kinematic
  []
  [tb4_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 8'
    weights = '-1 1 1'
    secondary_node_ids = 114
    penalty = 1e9
    formulation = kinematic
  []
  [tb4_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 8'
    weights = '-1 1 1'
    secondary_node_ids = 114
    penalty = 1e9
    formulation = kinematic
  []
  [tb5_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 10'
    weights = '-1 1 1'
    secondary_node_ids = 115
    penalty = 1e9
    formulation = kinematic
  []
  [tb5_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 10'
    weights = '-1 1 1'
    secondary_node_ids = 115
    penalty = 1e9
    formulation = kinematic
  []
  [tb6_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 12'
    weights = '-1 1 1'
    secondary_node_ids = 116
    penalty = 1e9
    formulation = kinematic
  []
  [tb6_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 12'
    weights = '-1 1 1'
    secondary_node_ids = 116
    penalty = 1e9
    formulation = kinematic
  []
  [tb7_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 14'
    weights = '-1 1 1'
    secondary_node_ids = 117
    penalty = 1e9
    formulation = kinematic
  []
  [tb7_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 14'
    weights = '-1 1 1'
    secondary_node_ids = 117
    penalty = 1e9
    formulation = kinematic
  []
  [tb8_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 16'
    weights = '-1 1 1'
    secondary_node_ids = 118
    penalty = 1e9
    formulation = kinematic
  []
  [tb8_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 16'
    weights = '-1 1 1'
    secondary_node_ids = 118
    penalty = 1e9
    formulation = kinematic
  []
  [tb9_x]
    type = LinearNodalConstraintFix
    variable = disp_x
    primary = '0 111 18'
    weights = '-1 1 1'
    secondary_node_ids = 119
    penalty = 1e9
    formulation = kinematic
  []
  [tb9_y]
    type = LinearNodalConstraintFix
    variable = disp_y
    primary = '0 111 18'
    weights = '-1 1 1'
    secondary_node_ids = 119
    penalty = 1e9
    formulation = kinematic
  []
[]
