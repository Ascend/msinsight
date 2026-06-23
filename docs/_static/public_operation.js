// 在页面加载完成后执行
document.addEventListener('DOMContentLoaded', function () {

    // 选择左侧菜单中所有带有id属性的a标签
    const domA = document.querySelectorAll('.wy-menu a[id]');

    // 移除所有筛选到的标签
    domA.forEach(function (a) {
        a.remove();
    });
});
