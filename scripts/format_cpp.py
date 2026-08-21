#!/usr/bin/env python3
"""
@file format_cpp.py
@brief 批量调用 clang-format 格式化项目自有 C++ 源码。

默认排除构建目录、第三方目录和生成目录；执行前应先查看 git diff，避免格式化无关文件。
"""

import os
import subprocess
import sys
from pathlib import Path

# 需要排除的目录列表
EXCLUDE_DIRS = {
    'build',
    'deps',
    'thirdparty',
    'third_party',
    'vendor',
    'node_modules',
    '.git',
    '.cache',
    'cmake-build-debug',
    'cmake-build-release',
    'out',
    'bin',
    'obj',
}

# C++文件扩展名
CPP_EXTENSIONS = {'.cpp', '.cc', '.cxx', '.c++', '.hpp', '.h', '.hxx', '.h++'}


def find_cpp_files(root_dir, exclude_dirs=None):
    """
    递归查找所有C++源文件
    
    Args:
        root_dir: 根目录路径
        exclude_dirs: 需要排除的目录集合
    
    Returns:
        找到的C++文件路径列表
    """
    if exclude_dirs is None:
        exclude_dirs = EXCLUDE_DIRS
    
    cpp_files = []
    root_path = Path(root_dir)
    
    for dirpath, dirnames, filenames in os.walk(root_path):
        # 获取当前目录名（相对于根目录）
        current_dir = Path(dirpath)
        
        # 排除指定的目录（原地修改dirnames，阻止os.walk进入这些目录）
        dirnames[:] = [
            d for d in dirnames 
            if d not in exclude_dirs and not d.startswith('.')
        ]
        
        # 查找C++文件
        for filename in filenames:
            file_path = Path(dirpath) / filename
            if file_path.suffix.lower() in CPP_EXTENSIONS:
                cpp_files.append(file_path)
    
    return cpp_files


def format_files(cpp_files, style='file', dry_run=False):
    """
    使用clang-format格式化文件
    
    Args:
        cpp_files: 文件路径列表
        style: clang-format样式（'file'表示使用.clang-format配置文件）
        dry_run: 如果为True，只显示将要格式化的文件，不实际执行
    
    Returns:
        (成功数量, 失败数量)
    """
    success_count = 0
    fail_count = 0
    
    print(f"\n找到 {len(cpp_files)} 个C++文件")
    
    if dry_run:
        print("\n[预览模式] 将要格式化的文件:")
        for file_path in cpp_files:
            print(f"  - {file_path}")
        return 0, 0
    
    print("\n开始格式化...")
    
    for i, file_path in enumerate(cpp_files, 1):
        try:
            # 执行clang-format命令
            cmd = ['clang-format', '-i', f'-style={style}', str(file_path)]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode == 0:
                success_count += 1
                print(f"[{i}/{len(cpp_files)}] ✓ {file_path}")
            else:
                fail_count += 1
                print(f"[{i}/{len(cpp_files)}] ✗ {file_path}")
                if result.stderr:
                    print(f"    错误: {result.stderr.strip()}")
        
        except subprocess.TimeoutExpired:
            fail_count += 1
            print(f"[{i}/{len(cpp_files)}] ✗ {file_path} (超时)")
        except FileNotFoundError:
            print("\n错误: 未找到clang-format命令")
            print("请确保已安装clang-format并添加到系统PATH中")
            sys.exit(1)
        except Exception as e:
            fail_count += 1
            print(f"[{i}/{len(cpp_files)}] ✗ {file_path} ({str(e)})")
    
    return success_count, fail_count


def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='批量格式化C++代码',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python format_cpp.py                    # 格式化当前目录
  python format_cpp.py /path/to/project   # 格式化指定目录
  python format_cpp.py --dry-run          # 预览将要格式化的文件
  python format_cpp.py --style=LLVM       # 使用LLVM样式
        """
    )
    
    parser.add_argument(
        'directory',
        nargs='?',
        default='.',
        help='要格式化的目录路径（默认: 当前目录）'
    )
    
    parser.add_argument(
        '--style',
        default='file',
        help='clang-format样式（默认: file，使用.clang-format配置文件）'
    )
    
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='预览模式，只显示将要格式化的文件，不实际执行'
    )
    
    parser.add_argument(
        '--exclude',
        nargs='+',
        default=[],
        help='额外要排除的目录名称'
    )
    
    args = parser.parse_args()
    
    # 检查目录是否存在
    target_dir = Path(args.directory).resolve()
    if not target_dir.exists():
        print(f"错误: 目录不存在: {target_dir}")
        sys.exit(1)
    
    if not target_dir.is_dir():
        print(f"错误: 路径不是目录: {target_dir}")
        sys.exit(1)
    
    print(f"目标目录: {target_dir}")
    print(f"格式化样式: {args.style}")
    
    # 合并排除目录
    exclude_dirs = EXCLUDE_DIRS.copy()
    exclude_dirs.update(args.exclude)
    print(f"排除目录: {', '.join(sorted(exclude_dirs))}")
    
    # 查找C++文件
    cpp_files = find_cpp_files(target_dir, exclude_dirs)
    
    if not cpp_files:
        print("\n未找到需要格式化的C++文件")
        sys.exit(0)
    
    # 格式化文件
    success_count, fail_count = format_files(cpp_files, args.style, args.dry_run)
    
    # 打印统计信息
    if not args.dry_run:
        print("\n" + "="*50)
        print(f"格式化完成!")
        print(f"成功: {success_count} 个文件")
        if fail_count > 0:
            print(f"失败: {fail_count} 个文件")
        print(f"总计: {len(cpp_files)} 个文件")
        print("="*50)


if __name__ == '__main__':
    main()
