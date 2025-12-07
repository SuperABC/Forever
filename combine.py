import os


def combine_code_files(output_file='combined_code.txt', extensions=None, ignore_dirs=None, ignore_files=None):
    """
    将所有代码文件合并成一个文本文件
    
    :param output_file: 输出文件名
    :param extensions: 要合并的代码文件扩展名列表
    :param ignore_dirs: 要忽略的文件夹列表
    :return: 合并的文件数量
    """
    if extensions is None:
        extensions = [
            '.py', '.js', '.java', '.c', '.cpp', '.h', '.hpp',
            '.go', '.rs', '.ts', '.sh', '.php', '.rb', '.swift',
            '.kt', '.kts', '.m', '.html', '.css', '.scss', '.less'
        ]

    if ignore_dirs is None:
        ignore_dirs = [
            'venv', '.git', '__pycache__', 'node_modules',
            '.idea', '.vscode', 'build', 'dist', 'target', 'SGL', 'WXDJ'
        ]

    if ignore_files is None:
        ignore_files = [
            'json.h', 'json.cpp', 'visualize.cpp', 'lines.py', 'combine.py'
        ]
        
    # 收集所有符合条件的文件
    code_files = []
    for root, dirs, files in os.walk('.'):
        # 移除要忽略的文件夹
        dirs[:] = [d for d in dirs if d not in ignore_dirs]
        
        for file in files:
            if any(file.endswith(ext) for ext in extensions) and file not in ignore_files:
                file_path = os.path.join(root, file)
                code_files.append(file_path)
    
    # 按文件名排序，使输出更有条理
    code_files.sort()
    
    file_count = 0
    
    # 写入输出文件
    with open(output_file, 'w', encoding='utf-8') as out_f:
        for file_path in code_files:
            try:
                # 写入文件名（作为分隔行）
                out_f.write(f"// ===== {file_path} =====\n\n")
                
                # 读取并写入文件内容
                with open(file_path, 'r', encoding='utf-8') as in_f:
                    content = in_f.read()
                    out_f.write(content)
                
                # 文件之间用空行分隔
                out_f.write("\n\n")
                file_count += 1
                
                print(f"已合并: {file_path}")
                
            except UnicodeDecodeError:
                print(f"警告: 无法解码文件（可能是二进制文件），跳过: {file_path}")
            except PermissionError as e:
                print(f"警告: 无权限读取文件，跳过 {file_path}: {e}")
            except Exception as e:
                print(f"警告: 读取文件 {file_path} 时发生未知错误，跳过: {e}")
    
    return file_count


if __name__ == '__main__':
    file_count = combine_code_files()
    print(f"\n已成功合并 {file_count} 个文件到 combined_code.txt")