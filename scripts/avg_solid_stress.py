import csv

xPhys = []
stress = []
# density in first column, stress in second column
with open("scripts/extract_elemental.csv", "r") as file:
    content = csv.reader(file)
    for line in content:
        xPhys.append(float(line[0]))
        stress.append(float(line[1]))

avgSolidStress = sum(stress) / sum(xPhys)
gValue = avgSolidStress / max(stress) - 1
print("Stress sum", sum(stress))
print("Dens sum", sum(xPhys))
print("Average solid stress is ", avgSolidStress)
# print("Constraint value is ", gValue)

ssum_test = 0
dsum_test = 0
for i, dens in enumerate(xPhys):
    if dens > 0.9:
        ssum_test += stress[i]
        dsum_test += xPhys[i]
        # print(stress[i], xPhys[i])
print("\nTest stress sum is", ssum_test)
print("Test dens sum is", dsum_test)
avgSolidStress_test = ssum_test / dsum_test
print("Test avg solid stress is ", avgSolidStress_test)

ssum_low = 0
dsum_low = 0
for i, dens in enumerate(xPhys):
    if dens < 0.01:
        ssum_low += 5e-3
        # print(stress[i])
        dsum_low += xPhys[i]
print("\nStress sum in low density elements is", ssum_low)
print("Dens sum in low density elements is", dsum_low)
avgSolidStress_dbg = ssum_low / dsum_low
print("Average solid stress is ", avgSolidStress_dbg)
