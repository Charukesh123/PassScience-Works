"""Generates the two SELF-CREATED example files in data/ (build-time helper, not application logic)."""
import math
R, L, SEG = 0.0056419, 1.0, 24   # r = sqrt(A/pi) with A = 1e-4 m^2 ; rod along +x, SI units (m)
ring = lambda x: [(x, R*math.cos(2*math.pi*i/SEG), R*math.sin(2*math.pi*i/SEG)) for i in range(SEG)]
a, b = ring(0.0), ring(L)
tris = []
for i in range(SEG):
    j = (i+1) % SEG
    tris += [(a[i], b[i], b[j]), (a[i], b[j], a[j])]               # side wall
    tris += [((0,0,0), a[j], a[i]), ((L,0,0), b[i], b[j])]          # end caps
def normal(t):
    (x1,y1,z1),(x2,y2,z2),(x3,y3,z3) = t; u=(x2-x1,y2-y1,z2-z1); v=(x3-x1,y3-y1,z3-z1)
    n=(u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]); m=math.sqrt(sum(c*c for c in n)) or 1
    return tuple(c/m for c in n)
with open('data/rod_cylinder.stl','w', newline='\n') as f:
    f.write('solid rod_cylinder\n')
    for t in tris:
        n = normal(t); f.write(f' facet normal {n[0]:.6e} {n[1]:.6e} {n[2]:.6e}\n  outer loop\n')
        for v in t: f.write(f'   vertex {v[0]:.6e} {v[1]:.6e} {v[2]:.6e}\n')
        f.write('  endloop\n endfacet\n')
    f.write('endsolid rod_cylinder\n')
# SYNTHETIC nodal temperatures: analytic rod profile sampled on 21 axial stations x (centre + 8 ring nodes)
T = lambda x: 100 - 80*x/L + (1e4/(2*10))*x*(L-x)
with open('data/ansys_synthetic_nodal_temperature.csv','w', newline='\n') as f:
    f.write('node_id,x_m,y_m,z_m,temperature_C\n'); nid = 1
    for s in range(21):
        x = L*s/20; pts = [(0.0,0.0)] + [(R*math.cos(2*math.pi*k/8), R*math.sin(2*math.pi*k/8)) for k in range(8)]
        for (y,z) in pts:
            f.write(f'{nid},{x:.6f},{y:.6e},{z:.6e},{T(x):.6f}\n'); nid += 1
