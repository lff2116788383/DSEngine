"""检查 loading.png sprite sheet 每帧内容的垂直位置偏移"""
from PIL import Image
import numpy as np

img = Image.open('examples/KF_Framework/assets/textures/loading.png')
w, h = img.size
print(f'Image size: {w}x{h}')
cell_w = w // 2
cell_h = h // 15
print(f'Cell size: {cell_w}x{cell_h}')
data = np.array(img)

print()
print('Row | Col0 top/bot/center | Col1 top/bot/center | Y-diff')
print('-' * 70)

for row in range(15):
    for col in range(2):
        cell = data[row*cell_h:(row+1)*cell_h, col*cell_w:(col+1)*cell_w]
        # 检查非透明像素 (alpha通道或亮度)
        if cell.shape[2] == 4:
            mask = cell[:,:,3] > 10
        else:
            mask = np.any(cell[:,:,:3] > 30, axis=2)
        
        row_has_content = mask.sum(axis=1) > 0
        rows_with_content = np.where(row_has_content)[0]
        
        if len(rows_with_content) > 0:
            top = rows_with_content[0]
            bot = rows_with_content[-1]
            center = (top + bot) / 2.0
        else:
            top = bot = center = -1
        
        if col == 0:
            c0_top, c0_bot, c0_center = top, bot, center
        else:
            c1_top, c1_bot, c1_center = top, bot, center
    
    diff = c1_center - c0_center if c0_center >= 0 and c1_center >= 0 else 0
    print(f'  {row:2d} | {c0_top:3d}/{c0_bot:3d}/{c0_center:5.1f} | {c1_top:3d}/{c1_bot:3d}/{c1_center:5.1f} | {diff:+.1f}')

# 统计所有帧的 center Y
print()
centers = []
for row in range(15):
    for col in range(2):
        cell = data[row*cell_h:(row+1)*cell_h, col*cell_w:(col+1)*cell_w]
        if cell.shape[2] == 4:
            mask = cell[:,:,3] > 10
        else:
            mask = np.any(cell[:,:,:3] > 30, axis=2)
        rows_with_content = np.where(mask.sum(axis=1) > 0)[0]
        if len(rows_with_content) > 0:
            centers.append((rows_with_content[0] + rows_with_content[-1]) / 2.0)

centers = np.array(centers)
print(f'Center Y stats: min={centers.min():.1f}, max={centers.max():.1f}, range={centers.max()-centers.min():.1f} px')
print(f'Mean={centers.mean():.1f}, std={centers.std():.1f}')
