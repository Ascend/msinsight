#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import re


def read_file(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        return f.read()


def write_file(file_path, content):
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)


def convert_path(path):
    if path.startswith('http://') or path.startswith('https://') or path.startswith('#'):
        return path
    if path.startswith('./docs/zh/'):
        return './zh/' + path[10:]
    if path.startswith('./'):
        return 'https://raw.gitcode.com/Ascend/msinsight/files/master/' + path[2:]
    return path


def replace_path(match):
    return match.group(1) + convert_path(match.group(2)) + match.group(3)


def convert_paths(content):
    content = re.sub(r'\./zh/', './', content)
    content = re.sub(r'(\]\()([^)]+)(\))', replace_path, content)
    content = re.sub(r'(src=")([^"]+)(")', replace_path, content)
    return content


def remove_sections(content, titles_to_remove):
    lines = content.split('\n')
    result_lines = []
    skip = False
    current_level = 0
    skip_details = False
    
    for i, line in enumerate(lines):
        if '<details>' in line:
            next_line_idx = i + 1
            if next_line_idx < len(lines):
                next_line = lines[next_line_idx]
                summary_match = re.search(r'<summary[^>]*>(.+?)</summary>', next_line)
                if summary_match:
                    title = summary_match.group(1).strip()
                    if title in titles_to_remove:
                        skip = True
                        skip_details = True
                        while result_lines and result_lines[-1].strip() == '':
                            result_lines.pop()
                        continue
        
        if skip_details and '</details>' in line:
            skip = False
            skip_details = False
            continue
        
        header_match = re.match(r'^(#{1,6})\s+(.+)$', line)
        
        if header_match:
            level = len(header_match.group(1))
            title = header_match.group(2).strip()
            
            if title in titles_to_remove:
                skip = True
                current_level = level
                while result_lines and result_lines[-1].strip() == '':
                    result_lines.pop()
                continue
            elif skip and level <= current_level:
                skip = False
        
        if skip:
            continue
        
        result_lines.append(line)
    
    return '\n'.join(result_lines)


def main():
    titles_to_remove = ['Demo', '🗺️ 目录结构', '🚀 快速入门', '🛠️ 贡献指南']
    
    readme_path = 'README.md'
    index_path = 'docs/index.md'
    
    readme_content = read_file(readme_path)
    index_content = read_file(index_path)
    
    if titles_to_remove:
        readme_content = remove_sections(readme_content, titles_to_remove)
    
    processed_readme = convert_paths(readme_content)
    
    new_index_content = processed_readme + '\n'.join(index_content.splitlines()[1:])
    
    write_file(index_path, new_index_content)
    
    print(f"Successfully processed README.md and updated {index_path}")


if __name__ == '__main__':
    main()