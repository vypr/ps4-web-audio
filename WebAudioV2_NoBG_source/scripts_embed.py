#!/usr/bin/env python3
import sys
from pathlib import Path
source=Path(sys.argv[1]).read_bytes()
out=Path(sys.argv[2]); sym=sys.argv[3]
out.write_text('#ifndef WA_WEB_EMBED\n#define WA_WEB_EMBED\n'+
               f'static const unsigned char {sym}[] = {{\n'+
               ''.join('  '+','.join(str(x) for x in source[i:i+24])+',\n' for i in range(0,len(source),24))+
               '};\n#endif\n')
