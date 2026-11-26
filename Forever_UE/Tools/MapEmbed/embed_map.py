"""把 .terrain / .roadnet 文件编码进一个 TerrainMod / RoadnetMod 的 C++ 源码，生成的 mod 不读文件就能造出同样的地形/路网。

生成的代码以 Basic 的 terrain_file / roadnet_file 为模板：整份拷过去，只改类名/id/显示名，并把
`// [map_embed:load begin]` 到 `// [map_embed:load end]` 之间"按 --path 读文件"的那段换成"从内嵌数组取数据"。
所以解析逻辑只有 Basic 里一份，改了 terrain_file/roadnet_file 之后重新跑本工具即可同步。详见 README.md。

用法：
  python embed_map.py terrain <in.terrain> --id terrain_small --class SmallTerrain --name 小地图地形 --out <目录>
  python embed_map.py roadnet <in.roadnet> --id roadnet_small --class SmallRoadnet --name 小地图路网 --out <目录>
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BASIC_MAP = os.path.normpath(os.path.join(HERE, "..", "..", "Source", "Basic", "map"))

KINDS = {
    "terrain": {
        "template": "terrain_file",
        "class": "FileTerrain",
        "display": "文件地形",
        "concept": "Terrain",
        # 换掉读文件那段后，模板后面的代码从 data(vector<char>) 读。
        "load": "\tdata.assign(reinterpret_cast<const char*>(kEmbeddedData), reinterpret_cast<const char*>(kEmbeddedData) + sizeof(kEmbeddedData));\n",
    },
    "roadnet": {
        "template": "roadnet_file",
        "class": "FileRoadnet",
        "display": "文件路网",
        "concept": "Roadnet",
        # 换掉读文件那段后，模板后面的代码从 text(string) 解析 JSON。
        "load": "\tstring text(reinterpret_cast<const char*>(kEmbeddedData), sizeof(kEmbeddedData));\n",
    },
}

LOAD_RE = re.compile(r"[ \t]*// \[map_embed:load begin\][^\n]*\n.*?[ \t]*// \[map_embed:load end\][^\n]*\n", re.S)


def fail(message):
    sys.exit("embed_map: " + message)


def replace_once(text, old, new, what):
    count = text.count(old)
    if count == 0:
        fail(f"模板里找不到 {what}（{old!r}），Basic 的 {what} 写法变了，需要同步更新 embed_map.py")
    return text.replace(old, new)


def read_payload(kind, path):
    with open(path, "rb") as f:
        raw = f.read()
    if kind == "terrain":
        if raw[:4] != b"FTRN":
            fail(f"{path} 不是 .terrain 文件（魔数不是 FTRN）")
        return raw
    # .roadnet：mod 只读 output 段，source 段是编辑器自己的编辑模型，去掉并压紧以缩小源码。
    try:
        root = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        # 宽松 JSON(注释/尾逗号) Python 读不了，就原样内嵌，mod 那边的 JsonReader 能读。
        print(f"embed_map: {path} 不是严格 JSON，原样内嵌", file=sys.stderr)
        return raw
    if not isinstance(root, dict) or not isinstance(root.get("output"), dict):
        fail(f"{path} 没有 output 段，不是 MapEditor 导出的 .roadnet")
    return json.dumps({"output": root["output"]}, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def write_data(path, payload, source_name):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(f"// 自动生成(Tools/MapEmbed/embed_map.py)，勿手改。内容来自 {source_name}，{len(payload)} 字节。\n")
        f.write("static const unsigned char kEmbeddedData[] = {\n")
        for start in range(0, len(payload), 32):
            f.write(",".join(str(b) for b in payload[start:start + 32]))
            f.write(",\n" if start + 32 < len(payload) else "\n")
        f.write("};\n")


def generate(kind, input_path, mod_id, class_name, display_name, out_dir):
    spec = KINDS[kind]
    template = spec["template"]
    with open(os.path.join(BASIC_MAP, template + ".h"), encoding="utf-8") as f:
        header = f.read()
    with open(os.path.join(BASIC_MAP, template + ".cpp"), encoding="utf-8") as f:
        source = f.read()

    source_name = os.path.basename(input_path)
    banner = (f"// 自动生成(Tools/MapEmbed/embed_map.py {kind} {source_name} --id {mod_id} --class {class_name})，勿手改：\n"
              f"// 以 Source/Basic/map/{template}.* 为模板，数据内嵌在 {mod_id}_data.inc，不读文件。改 {template} 后重新生成。\n")

    # .cpp：读文件那段 -> 内嵌数组；去掉 --path 参数；id/显示名/类名。
    if not LOAD_RE.search(source):
        fail(f"{template}.cpp 里找不到 [map_embed:load begin/end] 标记")
    source = LOAD_RE.sub(lambda _: spec["load"], source, count=1)
    source = replace_once(source, f'#include "{template}.h"', f'#include "{mod_id}.h"\n\n#include "{mod_id}_data.inc"', "头文件 include")
    source = replace_once(source, 'path = ModArgs(args).GetString("path");', f'path = "{source_name} (内嵌)";', "--path 参数解析")
    source = replace_once(source, f'return "{template}";', f'return "{mod_id}";', "GetId/GetType 返回值")
    source = replace_once(source, f'"{spec["display"]}"', f'"{display_name}"', "GetName 显示名")
    source = re.sub(rf"\b{spec['class']}\b", class_name, source)
    source = banner + "\n" + source

    # .h：类名，去掉讲 --path 的注释行(这个 mod 不收 --path)，把"读文件"的类注释换成生成说明。
    header = re.sub(rf"\b{spec['class']}\b", class_name, header)
    header = "\n".join(line for line in header.split("\n") if not (line.lstrip().startswith("//") and "--path" in line))
    header = re.sub(r"^// 读文件[^\n]*$", f"// {display_name}：{source_name} 内嵌进代码的{kind} mod，见{mod_id}.md。", header, count=1, flags=re.M)
    header = banner + "\n" + header

    payload = read_payload(kind, input_path)
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, mod_id + ".h"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
    with open(os.path.join(out_dir, mod_id + ".cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write(source)
    write_data(os.path.join(out_dir, mod_id + "_data.inc"), payload, source_name)
    with open(os.path.join(out_dir, mod_id + ".md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(f"# {mod_id}\n\n"
                f"## 职责\n{display_name}：把 `{source_name}` 编码进代码的 {kind} mod，效果与 `\"{template} --path=<{source_name}>\"` 相同，但不读文件、没有 `--path` 参数。\n\n"
                f"## 关键设计\n- 由 `Forever_UE/Tools/MapEmbed/embed_map.py` 生成，`{mod_id}.h/.cpp` 是 `Source/Basic/map/{template}.*` 的拷贝(只换类名/id/显示名、读文件段)，"
                f"数据在 `{mod_id}_data.inc`({len(payload)} 字节)。**勿手改**：`{source_name}` 或 `{template}` 改了就重新生成。\n"
                f"- 生成命令：`python Forever_UE/Tools/MapEmbed/embed_map.py {kind} <{source_name}路径> --id {mod_id} --class {class_name} --name {display_name} --out <本目录>`\n"
                f"- 设计细节(格式、优先级、贴图/tag 等)同 `Source/Basic/map/{template}.md`。\n\n"
                f"## 依赖关系\n只依赖 Dependence。\n")

    concept = spec["concept"]
    print(f"已生成 {out_dir}\\{mod_id}.h/.cpp/.md/_data.inc（{len(payload)} 字节）。在所在 DLL 里注册：")
    print(f'  #include "{mod_id}.h"')
    print(f"  GetMod{concept}s:      {class_name}::GetId()")
    print(f"  RegisterMod{concept}s: factory->Register{concept}({class_name}::GetId(),")
    print(f"      [](const std::string& args) -> {concept}Mod* {{ return new {class_name}(args); }},")
    print(f"      []({concept}Mod* m) {{ delete m; }});")


def main():
    parser = argparse.ArgumentParser(description="把 .terrain/.roadnet 编码进 TerrainMod/RoadnetMod 的 C++ 源码")
    parser.add_argument("kind", choices=sorted(KINDS))
    parser.add_argument("input", help=".terrain 或 .roadnet 文件")
    parser.add_argument("--id", required=True, help="mod id，如 terrain_small；也是生成的文件名")
    parser.add_argument("--class", dest="class_name", required=True, help="C++ 类名，如 SmallTerrain")
    parser.add_argument("--name", required=True, help="GetName 的显示名前缀，如 小地图地形")
    parser.add_argument("--out", required=True, help="输出目录(通常是某个 mod DLL 的源码目录)")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z_]\w*", args.class_name):
        fail(f"类名 {args.class_name!r} 不是合法的 C++ 标识符")
    if not re.fullmatch(r"[\w.-]+", args.id):
        fail(f"id {args.id!r} 要能当文件名用")
    generate(args.kind, args.input, args.id, args.class_name, args.name, args.out)


if __name__ == "__main__":
    main()
