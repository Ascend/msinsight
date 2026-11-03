# memscope

Independent memory frontend application. Owns API, state, loaders, notifications, page and rendering resources.

`src/common` is an application-local copy of the former memory-common implementation. It is not a shared package; changes in one application do not affect the other. No new capabilities are extracted into lib in this stage. Both applications continue to use existing @insight/lib infrastructure.

Development port: 3007. Build output: `memscope/build`. Host plugin: MemScope.

Run from modules: `pnpm --filter @insight/memscope start` or `pnpm --filter @insight/memscope build`.

The existing navigation selects the application by dataset. Backend routing remains leaks. The legacy leaks frontend is removed. VS Code compatibility for these independent applications is outside this change; existing extension adapters are not upgraded.

## Integration regression

Build both applications, then run `pnpm --filter @insight/memscope test:integration` from modules. Requires installed Edge (or the Playwright channel selected by MEMORY_TEST_BROWSER). Uses a synthetic host/dataset and both actual production bundles to check initial loading, snapshot window switching, return to Scope and request/API isolation. It does not verify real backend parsing.
