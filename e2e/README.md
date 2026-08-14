# MindStudio Insight GUI Test

## Project Setup

### 1、安装依赖

```sh
npm install
```

### 2、下载浏览器二进制文件

```sh
npx playwright install
```

### 3、准备测试数据

测试数据可以在数据仓库中下载: [下载测试数据](https://gitcode.com/zhangruoyu2/msinsight-quick-start-demo.git)

在utils/constants.ts 中可以定义测试文件路径，目前所有测试数据都放在 C:\msinsight-quick-start-demo\GUI-test-data 路徑测试 目录下，请自行创建该目录或将配置更改为本地文件路径。

### 4、运行用例

运行GUI前，请分别完成前端和后端的构建。

```sh
# 运行所有用例
npm run test
# 运行预冒烟用例
npm run test:smoke
# 在 UI 模式下运行
npm run test -- --ui
# 运行单个测试文件
npm run test timeline.spec.ts
# 运行单个测试用例
npm run test -- -g test_unitsExpandAndCollapse_when_click
# 查看测试报告
npx playwright show-report
# 更新某个测试文件所有快照
npx playwright test tests/full-test/framework.spec.ts -u
# 更新某个测试用例快照
npx playwright test -g test_lock_selected_area_then_click_operator_detail -u
```

### 自动化写用例

```sh
npx playwright codegen localhost:5174 --viewport-size=1920,1080
```

### Lint with [ESLint](https://eslint.org/)

```sh
npm run lint
```

### ⚠️ 注意
>
> 1. 默认运行模式是无头模式，也就是不会有浏览器界面出现，如果为了方便调试与定位，可以更改playwright.config.ts配置，将headless改为false
> 2. 由于无头模式与有头模式下快照有差异，为确保用例的一致性，生成快照时必须要设置 headless: true
> 3. 运行前请关闭浏览器中已打开的 Insight 页面，否则无法建立 ws 连接，这是因为 ws 同时只能保持一个连接

### 建议

#### 1、定位器选择

不要使用不稳定的类名，如css-4lza26, 它是由Emotion库自动生成的，可能会在每次构建时发生变化，因此具有不稳定性。
优先使用确定的id、class、attribute选择器、以及官方内置的定位器：
> page.getByRole()：用于根据显式和隐式辅助功能属性进行定位。
> 
> page.getByText()：用于根据文本内容进行定位。
> 
> page.getByLabel()：用于根据关联标签的文本定位表单控件。
> 
> page.getByPlaceholder()：用于根据占位符定位输入框。
> 
> page.getByAltText()： 用于根据备用文本定位元素，通常是图像。
> 
> page.getByTitle()：用于根据 title 属性定位元素。
>
如果仍然无法获取到元素，可以给节点增加data-testid来辅助定位：
> page.getByTestId()：用于根据元素的 `data-testid` 属性进行定位（可以配置其他属性）。

#### 2、不建议使用 page.waitForTimeout()

page.waitForTimeout() 只能用于调试。在生产中使用计时器进行的测试不稳定，而且会增加用例执行时间。建议使用网络事件、选择器可见等方式来代替。

#### 3、缩小快照范围

在使用快照断言时，尽量将快照范围缩小到功能影响区域内，比如在测试图表功能时，只需要比对图表本身的变化即可。另外，在快照比对前，可以将鼠标移出截图区域，避免影响截图。

```js
 await page.mouse.move(0, 0);
 await expect(timelineFrame.locator('#main-container')).toHaveScreenshot('unit-offset.png');
```

#### 4、快照需要在无头模式下更新

因为无头模式下的页面是没有滚动条的，所以为了保持用例一致性，更新快照需要在无头模式下进行。

#### 5、并行、串行

默认test都是并行执行的，如果想要某些tests串行执行，可以在这组describe内设置test.describe.configure({ mode: 'serial' });

### 常见问题

**Q: 如何禁用无头模式？**

A: 在 playwright.config.ts 配置文件中，将 headless 设置为 false，即可显示浏览器窗口。

```ts
use: {
    headless: false
}
```
