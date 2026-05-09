import struct, sys
path = sys.argv[1] if len(sys.argv) > 1 else r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\MATERIAL\demoField.material"
f = open(path, "rb")
d = f.read(); f.close()
off = 0
for i in range(16):
    slen = struct.unpack_from('<i', d, off)[0]; off += 4
    name = d[off:off+slen].decode('ascii', errors='replace') if slen > 0 else ''
    off += slen
    if name: print(f"  tex[{i}]: '{name}'")
print(f"After strings: offset={off}")
a = struct.unpack_from('<4f', d, off); off += 16
diff = struct.unpack_from('<4f', d, off); off += 16
spec = struct.unpack_from('<4f', d, off); off += 16
emis = struct.unpack_from('<4f', d, off); off += 16
power = struct.unpack_from('<f', d, off)[0]
print(f"ambient:  ({a[0]:.3f},{a[1]:.3f},{a[2]:.3f},{a[3]:.3f})")
print(f"diffuse:  ({diff[0]:.3f},{diff[1]:.3f},{diff[2]:.3f},{diff[3]:.3f})")
print(f"specular: ({spec[0]:.3f},{spec[1]:.3f},{spec[2]:.3f},{spec[3]:.3f})")
print(f"emissive: ({emis[0]:.3f},{emis[1]:.3f},{emis[2]:.3f},{emis[3]:.3f})")
print(f"power:    {power:.3f}")

# KF shader formula:
# diffuse = material_diffuse * half_lambert * light_diffuse
# fully lit (hl=1): diffuse.r = diff[0] * 0.8
# DSE shader formula:
# diffuse = u_material_albedo * half_lambert * u_light_color * u_light_intensity
# fully lit (hl=1): diffuse = albedo * 0.92 * 0.85 = albedo * 0.782
print()
print("=== Comparison ===")
print(f"KF  fully-lit R: {diff[0]:.3f} * 1.0 * 0.8 = {diff[0]*0.8:.3f}")
print(f"DSE fully-lit R: 1.0 * 1.0 * 0.92 * 0.85 = {0.92*0.85:.3f}")
print(f"Ratio DSE/KF: {0.92*0.85/(diff[0]*0.8):.3f}" if diff[0] > 0 else "KF diffuse is 0!")
