# memsnapshot

Independent memory frontend application. Owns API, state, loaders, notifications, page and rendering resources.

`src/common` is an application-local copy of the former memory-common implementation. It is not a shared package; changes in one application do not affect the other. No new capabilities are extracted into lib in this stage. Both applications continue to use existing @insight/lib infrastructure.

Development port: 3011. Build output: `memsnapshot/build`. Host plugin: MemSnapshot.

Run from modules: `pnpm --filter @insight/memsnapshot start` or `pnpm --filter @insight/memsnapshot build`.

The existing navigation selects the application by dataset. Backend routing remains leaks. The legacy leaks frontend is removed. VS Code compatibility for these independent applications is outside this change; existing extension adapters are not upgraded.
