#!/bin/bash

TOOLCHAIN_ENV="/opt/fsl-imx-x11/4.1.15-2.1.0/environment-setup-cortexa7hf-neon-poky-linux-gnueabi"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 仅构建综合桌面和 000final 中的四个最终应用。
PROJECT_PRO_FILES=(
    "systemui/systemui.pro"
    "000final/001musicV2/musicV2/musicV2.pro"
    "000final/002weather_qml/weather_qml/weather_qml.pro"
    "000final/003mapTestLite/mapviewer/mapviewer.pro"
    "000final/004camrecorder/camrecorder/camrecorder.pro"
)

# SystemUI 保持原地构建；四个应用统一使用各自的 000crossBuild 目录。
PROJECT_BUILD_DIRS=(
    "systemui"
    "000final/001musicV2/000crossBuild"
    "000final/002weather_qml/000crossBuild"
    "000final/003mapTestLite/000crossBuild"
    "000final/004camrecorder/000crossBuild"
)

if [ ! -f "$TOOLCHAIN_ENV" ]; then
    echo "警告：未找到正点原子 IMX6ULL Qt 交叉编译环境。"
    echo "请先安装工具链：$TOOLCHAIN_ENV"
    exit 1
fi

source "$TOOLCHAIN_ENV"

# 编译综合桌面和四个最终应用，任一项目失败即停止。
do_compile() {
    local index
    local pro_file
    local build_dir

    for ((index = 0; index < ${#PROJECT_PRO_FILES[@]}; index++)); do
        pro_file="$PROJECT_ROOT/${PROJECT_PRO_FILES[$index]}"
        build_dir="$PROJECT_ROOT/${PROJECT_BUILD_DIRS[$index]}"

        mkdir -p "$build_dir"
        echo "正在编译 ${PROJECT_PRO_FILES[$index]}"
        (
            cd "$build_dir" || exit 1
            qmake "$pro_file" || exit 1
            make -j4
        ) || return 1
    done
}

# 清理五个受管理项目的构建产物，不扫描其他历史目录。
do_cleanall() {
    local build_dir

    for build_dir in "${PROJECT_BUILD_DIRS[@]}"; do
        build_dir="$PROJECT_ROOT/$build_dir"
        if [ -f "$build_dir/Makefile" ]; then
            echo "正在清理 $build_dir"
            (
                cd "$build_dir" || exit 1
                make distclean
            ) || return 1
        fi
    done
}

# 显示脚本支持的命令。
usage() {
    echo "用法：$(basename "${BASH_SOURCE[0]}") [all|cleanall|help]"
    echo "  all       编译 SystemUI 和四个最终应用"
    echo "  cleanall  清理五个受管理项目"
    echo "  help      显示帮助"
}

option="${1:-all}"
case "$option" in
    all)
        echo "开始编译，生成文件将复制到 ui 和 ui/src/apps。"
        if do_compile; then
            echo "编译完成。将整个 ui 文件夹复制到开发板 /opt 后运行 /opt/ui/systemui。"
        else
            echo "编译失败，请检查上方首个错误。"
            exit 1
        fi
        ;;
    cleanall)
        do_cleanall
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        echo "无效参数：$option"
        usage
        exit 1
        ;;
esac
