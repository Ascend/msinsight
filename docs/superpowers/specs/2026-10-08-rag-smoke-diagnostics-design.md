# Required RAG Smoke Diagnostics

## Approved Scope

Required build smoke tests emit startup diagnostics and failure details by default.
ONNX import and session initialization retain their original exceptions as Error.cause.
The existing fail-closed behavior, exit codes, and successful stdout JSON remain intact.

## Design

- A shared smoke diagnostics module formats bounded error cause chains and redacts
  queries, credential markers, knowledge text, and model/data/runtime paths.
- The build wrapper writes Node version, executable, platform, architecture, bundle
  location, resolved ONNX entry/version, and target native file existence/size to stderr.
- The smoke entry records its current phase. Native load and initialization failures
  report the more precise ONNX phase plus the original error name/code/message/stack.
- The wrapper forwards bounded, redacted child stderr and reports child exit status,
  termination signal, and spawn/timeout errors separately from the final failure.
- Product service logging remains stable; full causes are formatted by the build
  smoke entry rather than written by the embedding runtime itself.

## Ephemeral CI Workspace Probes

When the child reports native_runtime_load_failed at onnx_import, the wrapper runs
failure-only probes before returning, so evidence survives workspace cleanup:

- Separate Node processes use process.execPath to import the source dependency and
  the exact packaged entry. Each reports resolution, package version, native file
  SHA256/size, import outcome, and selected loaded runtime module paths.
- On Windows, an isolated Windows PowerShell process reports Windows build, CPU,
  process bitness, and VC++ DLL candidates in System32, Node, native, and PATH
  directories. Only matching DLLs are reported; the full environment is not dumped.
- The Windows probe calls LoadLibraryExW on the packaged onnxruntime.dll using its
  own directory plus System32, performs real initialization, and records the numeric
  Win32 error immediately. This controlled-search PowerShell result is distinct from
  Node's own loading behavior and is not a substitute for the original smoke result.
- All subprocesses share a 60-second budget, with at most 20 seconds per probe.
  Missing tools, crashes, timeouts, partial output, and exhausted budgets are logged.
  Probe failure never replaces the original build error. Output remains bounded and
  redacted on stderr; probes never provision files or modify runtime installations.

Probe tests cover failure gating, independent process results, digest differences,
missing inputs, timeouts, exhausted budgets, and real Windows loader error reporting.

## Original Host And Native Compatibility Evidence

- The real smoke process records a whitelisted snapshot immediately before ONNX
  import and on failure: loaded module paths, Node/architecture, sanitized working
  directory, argument flag names, and memory usage. No full process report or
  environment is serialized. Observer failures cannot change runtime behavior.
- A bounded PE32/PE32+ parser reads machine type, minimum OS version, normal and
  delay imports, and named/ordinal symbols from file-backed RVAs. It requires no
  external compiler tools. Invalid images and inspection limits are explicit.
- Windows checks each dependency and required export in the controlled probe
  process. Executable-host imports are marked host-bound; optional delayed imports
  are not classified as mandatory import failures. API-set names are resolved by
  Windows rather than assumed missing because there is no same-named physical file.
- CPU feature results use IsProcessorFeaturePresent (AVX=39, AVX2=40) and explicitly
  retain its OS/HAL detection limitations. A false flag alone is not a CPU root cause.
- Structured conclusions separate confirmed observations, candidates, and evidence
  gaps. Error 1114 stays unresolved without loader/native exception evidence.
  Independent probe success does not invalidate the original process failure.

Reference specifications:
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent

## Verification

- Verify that actual ONNX import/session failures retain their cause and stable code.
- Verify diagnostic redaction, bounded/cyclic cause handling, and module resolution
  from the requested bundle rather than the invoking working directory.
- Exercise smoke entry failures and successful summaries with isolated fixtures.
- Exercise wrapper missing-input, child-failure, and child-success paths.
- Run existing RAG and packaged tests against a freshly rebuilt code-only bundle.

## Acceptance

CI logs distinguish missing JS modules from DLL loading failures and ONNX model
initialization failures without requiring a second diagnostic build. Diagnostics use
stderr; stdout remains a single machine-readable result. Sensitive values and
model/data/runtime absolute paths do not appear in failure diagnostics.
