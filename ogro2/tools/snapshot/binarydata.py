# Genera BinaryData.h/.cpp igual que juce_add_binary_data (para compilar la interfaz
# en una Mac sin CMake). Uso: python3 binarydata.py <carpeta_salida> <archivos...>
import os, re, sys
out, files = sys.argv[1], sys.argv[2:]
os.makedirs(out, exist_ok=True)
def ident(path):
    name = os.path.basename(path).replace(' ', '_').replace('.', '_')
    return re.sub(r'[^A-Za-z0-9_]', '', name)
with open(os.path.join(out, 'BinaryData.h'), 'w') as h, open(os.path.join(out, 'BinaryData.cpp'), 'w') as c:
    h.write('#pragma once\nnamespace BinaryData\n{\n')
    c.write('#include "BinaryData.h"\nnamespace BinaryData\n{\n')
    for f in files:
        data = open(f, 'rb').read()
        n = ident(f)
        h.write('    extern const char* %s;\n    const int %sSize = %d;\n' % (n, n, len(data)))
        c.write('static const unsigned char %s_data[] = {%s};\nconst char* %s = (const char*) %s_data;\n'
                % (n, ','.join(str(b) for b in data), n, n))
    h.write('}\n'); c.write('}\n')
