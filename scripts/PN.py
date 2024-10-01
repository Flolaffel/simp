import csv

P = 100
limit = 0.06

stress = []
with open("scripts/extract_elemental.csv", "r") as file:
    content = csv.reader(file)
    for lines in content:
        for line in lines:
            stress.append(float(line))

pn = 0
pn_rel = 0
max = 0
for value in stress:
    pn += (value) ** P
    pn_rel += (value / limit) ** P
    if value > max:
        max = value
pn = pn ** (1 / P)
pn_rel = pn_rel ** (1 / P)
# pn = pn ** (1 / P) / limit

print("max is", max)
print("PN is", pn)
print("standardized PN is", pn_rel)
print("g is", pn_rel - 1)
