/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import assert from "node:assert/strict";
import test from "node:test";
import { formatToolError, normalizeToolError, toModelFacingToolError } from "../../utils/toolError.mjs";
import { boundedDiagnosticText, fitDiagnosticRecord, redactDiagnosticText, sliceDiagnosticText } from "../../utils/toolErrorBudget.mjs";
import { TOOL_ERROR_LIMITS } from "../../utils/toolErrorPolicy.mjs";

const context = {
    diagnosticId: "00000000-0000-4000-8000-000000000001",
    toolName: "pt_snap", source: "cli", stage: "execute",
    outcome: "failed", retryAdvice: "correct_input",
};

for (const text of [
    "\n".repeat(5000), '"\\\t'.repeat(2000), "\u0000".repeat(5000),
    "\u{1f680}".repeat(5000), "x".repeat(100000),
]) test(`budgets use actual JSON length for escaped/Unicode input ${text.charCodeAt(0)}`, () => {
    const shape = normalizeToolError({
        code: "CLI_EXIT_NONZERO",
        message: text,
        retryable: false,
        details: { exitCode: 2, stderr: `${text}\nSPECIFIC_END`, stdout: text },
    }, context);
    assert.ok(shape.message.length <= TOOL_ERROR_LIMITS.message);
    assert.ok(JSON.stringify(shape.details).length <= TOOL_ERROR_LIMITS.details);
    assert.match(shape.details.stderr, /SPECIFIC_END$/);
    const formatted = formatToolError(shape);
    assert.ok(formatted.length <= TOOL_ERROR_LIMITS.text);
    assert.ok(formatted.includes("CLI_EXIT_NONZERO"));
    assert.ok(formatted.includes("SPECIFIC_END"));
    assert.doesNotThrow(() => JSON.parse(JSON.stringify(shape)));
});

test("a long message cannot push the specific stderr out of the model text", () => {
    const shape = normalizeToolError({
        code: "CLI_EXIT_NONZERO", message: "x".repeat(12000),
        details: { stderr: "specific table failure", exitCode: 2 },
    }, context);
    assert.equal(shape.details.truncation.message, true);
    const formatted = formatToolError(shape);
    assert.ok(formatted.length <= TOOL_ERROR_LIMITS.text);
    assert.ok(formatted.includes("specific table failure"));
});

test("slicing never leaves lone surrogate pairs", () => {
    assert.equal(sliceDiagnosticText("a\u{1f680}b", 2), "a");
    assert.equal(sliceDiagnosticText("a\u{1f680}b", 2, "tail"), "b");
});

test("all text channels redact credentials, URL passwords, user paths and terminal controls", () => {
    const raw = "Authorization: Bearer fixtureBearerValue api_key=fixtureKeyValue "
        + "https://demo:fixturePassValue@example.invalid/path?token=fixtureQueryValue "
        + "C:\\Users\\demo\\private.db /home/demo/private.db \u001b[31merror\u001b[0m";
    const clean = redactDiagnosticText(raw);
    for (const forbidden of ["fixtureBearerValue", "fixtureKeyValue", "fixturePassValue", "fixtureQueryValue", "\\Users\\demo", "/home/demo", "\u001b"]) {
        assert.ok(!clean.includes(forbidden), forbidden);
    }
    const shape = normalizeToolError({
        code: "CLI_EXIT_NONZERO", message: raw, details: { stdout: raw, stderr: raw },
    }, context);
    const text = formatToolError(shape);
    for (const forbidden of ["fixtureBearerValue", "fixtureKeyValue", "fixturePassValue", "fixtureQueryValue"]) {
        assert.ok(!text.includes(forbidden), forbidden);
    }
});

test("quoted credential values with spaces or JSON escapes are removed completely", () => {
    const raw = 'password="fixture spaced password" api_key=\'fixture spaced key\' '
        + '{"secret": "fixture \\"escaped\\" secret"}';
    const clean = redactDiagnosticText(raw);
    for (const fragment of ["fixture", "spaced password", "spaced key", "escaped"]) {
        assert.ok(!clean.includes(fragment), fragment);
    }
});

for (const quote of ['"', "'"]) test(`unterminated ${quote} credentials stay redacted after sampling and serialization`, () => {
    const secret = "fixtureUnterminatedSecret";
    for (const suffix of ["", "\\", "\\\\", "\nmore private text"]) {
        for (const long of [false, true]) {
            const raw = `password=${quote}${secret}${long ? "x".repeat(40000) : ""}${suffix}${long ? quote : ""}`;
            const shape = normalizeToolError({
                code: "CLI_EXIT_NONZERO", message: raw,
                details: { stdout: raw, stderr: raw },
            }, context);
            for (const output of [redactDiagnosticText(raw), JSON.stringify(shape), formatToolError(shape), toModelFacingToolError(shape, context).message]) {
                assert.ok(!output.includes(secret), output.slice(0, 100));
                assert.ok(!output.includes("private text"));
            }
            assert.ok(formatToolError(shape).length <= TOOL_ERROR_LIMITS.text);
            assert.ok(JSON.stringify(shape.details).length <= TOOL_ERROR_LIMITS.details);
        }
    }
});

test("redaction of complete or partial credentials remains idempotent", () => {
    for (const raw of ['token="fixtureSecret', "secret='fixtureSecret\\", 'password="fixtureSecret" stderr: missing TASK']) {
        const clean = redactDiagnosticText(raw);
        assert.ok(!clean.includes("fixtureSecret"));
        assert.equal(redactDiagnosticText(clean), clean);
    }
});

test("non-home absolute paths and non-HTTP URL passwords are not exposed by new CLI diagnostics", () => {
    const raw = 'D:\\private\\fixture.db "/data/private/fixture.db" '
        + 'ssh://demo:fixtureSshPassword@example.invalid/path file:///data/private/fixture.db';
    const clean = redactDiagnosticText(raw);
    for (const value of ["D:\\private", "/data/private", "fixtureSshPassword"]) {
        assert.ok(!clean.includes(value), value);
    }
});

test("tail sampling never exposes the end of a credential after losing its label", () => {
    const secret = `token=${"a".repeat(50000)}fixtureSecretEnd`;
    const fragment = boundedDiagnosticText(secret, 1600, "tail");
    assert.ok(!fragment.includes("fixtureSecretEnd"));
    assert.ok(fragment.includes("omitted"));
    assert.equal(boundedDiagnosticText(`${secret}\nSpecific database error`, 1600, "tail"), "Specific database error");
});

test("record fitting trims candidate arrays while keeping valid JSON and a marker", () => {
    const details = { availableSkills: Array(20).fill("long-skill-name"), diagnosticId: "fixture-id" };
    fitDiagnosticRecord(details, 160, ["diagnosticId"]);
    assert.ok(details.availableSkills.length < 20);
    assert.equal(details.truncation.availableSkills, true);
    assert.ok(JSON.stringify(details).length <= 160);
});

test("HTML bodies are not presented as trusted failure details", () => {
    const text = formatToolError(normalizeToolError({
        code: "CAPABILITY_RESPONSE_INVALID",
        message: "<html><body>fixtureHiddenValue</body></html>",
        details: { stderr: "<script>fixtureHiddenValue</script>", status: 502, contentType: "text/html" },
    }, { ...context, source: "capability_http", stage: "decode", outcome: "unknown" }));
    assert.ok(!text.includes("fixtureHiddenValue"));
    assert.ok(text.includes("502"));
});

test("typed identifiers and content-type fields cannot carry recognized secret formats", () => {
    const secret = "sk-fixtureabcdefghijklmnop";
    const command = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: "Unavailable.",
        details: { moduleId: secret, backendCode: secret },
        state: { activeModule: secret },
    }, { ...context, source: "command" });
    const network = normalizeToolError({
        code: "CAPABILITY_RESPONSE_INVALID", message: "Invalid response.",
        details: { contentType: `application/${secret}` },
    }, { ...context, source: "capability_http" });
    assert.ok(!JSON.stringify(command).includes(secret));
    assert.ok(!JSON.stringify(network).includes(secret));
});

test("cycles, huge arrays, BigInt and forbidden keys cannot bypass data budgets", () => {
    const state = { activeModule: "Compute", ready: false };
    state.loop = state;
    const shape = normalizeToolError({
        code: "COMMAND_UNAVAILABLE", message: "Unavailable.",
        details: {
            reason: "module_not_active", targetModule: "Timeline",
            arbitrary: Array(100000).fill("private"),
            backendCode: 2n,
        },
        state,
    }, { ...context, source: "command" });
    assert.ok(JSON.stringify(shape.details).length <= TOOL_ERROR_LIMITS.details);
    assert.ok(JSON.stringify(shape.state).length <= TOOL_ERROR_LIMITS.state);
    assert.ok(!JSON.stringify(shape).includes("private"));
    assert.ok(!JSON.stringify(shape).includes("loop"));
});

test("formatter handles malformed caller input without another exception", () => {
    for (const value of [undefined, null, 1, {}, Object.create(null)]) {
        const text = formatToolError(value);
        assert.equal(typeof text, "string");
        assert.ok(text.length > 0 && text.length <= TOOL_ERROR_LIMITS.text);
    }
});

test("auth scheme values are fully redacted (Basic, Digest, NTLM, Proxy-Authorization)", () => {
    const cases = [
        ["Authorization: Basic ZmFrZTpwYXNz", "ZmFrZTpwYXNz"],
        ["authorization: basic ZmFrZTpwYXNz", "ZmFrZTpwYXNz"],
        ["Proxy-Authorization: Digest ZmFrZURpZ2VzdA==", "ZmFrZURpZ2VzdA=="],
        ["Authorization: NTLM ZmFrZU5UTE0=", "ZmFrZU5UTE0="],
        ["Authorization: Bearer fixtureBearerTail", "fixtureBearerTail"],
    ];
    for (const [raw, secret] of cases) {
        const clean = redactDiagnosticText(raw);
        assert.ok(!clean.includes(secret), raw);
        assert.ok(clean.includes("[REDACTED]"), raw);
    }
    const raw = "Authorization: Basic ZmFrZTpwYXNz\nother failure";
    const shape = normalizeToolError({ code: "COMMAND_EXECUTION_FAILED", message: raw, details: { stderr: raw } }, context);
    assert.ok(!JSON.stringify(shape).includes("ZmFrZTpwYXNz"));
    assert.ok(!formatToolError(shape).includes("ZmFrZTpwYXNz"));
});

test("tail sampling omits multiline credentials whose label precedes the window", () => {
    const secret = "FIXTURE_SECRET_LINE";
    const multiline = "N".repeat(40000) + "\npassword=\"" + (secret + "\n").repeat(3000) + "\"\nfailed";
    const omitted = boundedDiagnosticText(multiline, 1600, "tail");
    assert.ok(!omitted.includes(secret));
    assert.ok(omitted.includes("omitted"));
    const inline = "N".repeat(40000) + "\npassword=\"" + secret + "\"\nfailed";
    const redacted = boundedDiagnosticText(inline, 1600, "tail");
    assert.ok(!redacted.includes(secret));
    assert.ok(!redacted.includes("omitted"));
});

test("successive diagnostics do not inherit credential scan state", () => {
    const first = "N".repeat(100000) + "\npassword=\"ok\"\nfailed";
    const second = "N".repeat(40000) + "\npassword=\"" + ("FIXTURE_SECRET_LINE\n").repeat(3000) + "\"\nfailed";
    boundedDiagnosticText(first, 1600, "tail");
    const out = boundedDiagnosticText(second, 1600, "tail");
    assert.ok(!out.includes("FIXTURE_SECRET_LINE"));
    assert.ok(out.includes("omitted"));
});
