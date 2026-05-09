import struct
f=open(r'c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\stage\demo.enemy','rb')
d=f.read(); f.close()
off=0
en=struct.unpack_from('<I',d,off)[0]; off+=4
param_sz = (len(d)-4)//en - 20
print(f'Enemy count: {en}, param_size={param_sz}')
for i in range(en):
    x,y,z=struct.unpack_from('<3f',d,off); off+=12
    wr=struct.unpack_from('<f',d,off)[0]; off+=4
    pr=struct.unpack_from('<f',d,off)[0]; off+=4
    print(f'  Enemy {i}: pos=({x:.2f}, {y:.2f}, {z:.2f}) warn={wr:.1f} patrol={pr:.1f}')
    params = struct.unpack_from(f'<{param_sz//4}f', d, off)
    print(f'    param: {[f"{p:.2f}" for p in params]}')
    off += param_sz
print(f'Parsed {off} of {len(d)} bytes')
