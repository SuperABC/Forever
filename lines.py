import os


def count_code_lines(extensions=None, ignore_dirs=None):
    """
    统计当前文件夹中代码文件的行数

    :param extensions: 要统计的代码文件扩展名列表，例如 ['.py', '.js', '.java']
                      如果为None，则默认统计常见的代码文件扩展名
    :param ignore_dirs: 要忽略的文件夹列表，例如 ['venv', '.git']
    :return: 总行数，以及每个文件的详细行数统计
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
            '.idea', '.vscode', 'build', 'dist', 'target', 'SGL'
        ]

    total_lines = 0
    file_stats = {}

    for root, dirs, files in os.walk('.'):
        # 移除要忽略的文件夹
        dirs[:] = [d for d in dirs if d not in ignore_dirs]

        for file in files:
            # 检查文件扩展名是否在目标列表中
            if any(file.endswith(ext) for ext in extensions):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        lines = sum(1 for line in f)
                        file_stats[file_path] = lines
                        total_lines += lines
                except UnicodeDecodeError:
                    print(f"警告: 无法解码文件（可能是二进制文件）: {file_path}")
                except PermissionError as e:
                    print(f"警告: 无权限读取文件 {file_path}: {e}")
                except Exception as e:
                    print(f"警告: 读取文件 {file_path} 时发生未知错误: {e}")

    return total_lines, file_stats


if __name__ == '__main__':
    total, details = count_code_lines()
    print("\n详细统计:")
    for file, lines in details.items():
        print(f"{file}: {lines} 行")
    print(f"总代码行数: {total}")
    print(f"总文件数: {len(details)}")