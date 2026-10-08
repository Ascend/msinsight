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
