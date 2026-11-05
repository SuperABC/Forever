"""把emcc产出的单文件js(wasm已base64内嵌)内联进html模板，得到一个可离线打开的html。"""
import sys

js_path, template_path, out_path = sys.argv[1:4]
js = open(js_path, encoding='utf-8').read().replace('</script', '<\\/script')
html = open(template_path, encoding='utf-8').read()
marker = '/*__MODULE_JS__*/'
assert marker in html
open(out_path, 'w', encoding='utf-8').write(html.replace(marker, js))
print('wrote', out_path, round(len(html) / 1024 + len(js) / 1024), 'KB')
