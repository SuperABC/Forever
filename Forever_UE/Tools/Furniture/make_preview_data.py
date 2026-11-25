# 把ModTest导出的家具布局json内嵌进preview.html，生成可直接双击打开的单文件页面：
#   set FURNITURE_DUMP=<任意路径>\layouts.json   (导出别的房间mod再设 FURNITURE_DUMP_MOD=<mod id>，默认room_residence)
#   x64\Release\ModTest.exe --config Resource\Config\config.json
#   python Tools/Furniture/make_preview_data.py <layouts.json> <输出的preview_data.html>
import os
import sys

src, dst = sys.argv[1], sys.argv[2]
here = os.path.dirname(os.path.abspath(__file__))
page = open(os.path.join(here, "preview.html"), encoding="utf-8").read()
data = open(src, encoding="utf-8").read()
page = page.replace('<script src="layouts.js"></script>', "<script>const LAYOUTS = " + data + ";</script>")
open(dst, "w", encoding="utf-8").write(page)
