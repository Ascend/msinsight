/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import {
    CHECKSUM_MEMBERS,
    validatePackageMemberBytes,
} from "../../../services/rag/wire/packageContracts.mjs";
import {
    createPackageV4Members,
    DOMAIN_DICTIONARY_TEXT,
    MODEL_CONTRACT,
    RUNTIME_CONTRACT,
    sha256,
} from "./packageV4Fixture.mjs";

test("minimal receiver accepts producer Profile, model, prompt, and Audit semantic changes", () => {
    const members = createPackageV4Members();
    mutateJsonMember(members, "manifest.json", (manifest) => {
        manifest.buildProfile = { profileId: "future-profile", sha256: "f".repeat(64) };
        manifest.summary.model = "future-provider/future-model";
        manifest.summary.opencodeVersion = "999.0.0";
        manifest.summary.promptVersion = "future-prompt";
        manifest.corpusPolicy.allowedDocumentCategories = ["future-category"];
    });
    members.set("build-audit.json", Buffer.from("opaque future producer audit\0bytes", "utf8"));
    rewriteChecksums(members);

    const pack = validate(members);

    assert.equal(pack.manifest.kbVersion, "26.1.1");
    assert.deepEqual(pack.buildAuditBytes, members.get("build-audit.json"));
});

test("minimal receiver ignores unknown producer metadata fields", () => {
    const members = createPackageV4Members();
    for (const name of ["manifest.json", "sources.jsonl", "documents.jsonl", "chunks.jsonl"]) {
        mutateJsonRecords(members, name, (value) => { value.futureProducerMetadata = { accepted: true }; });
    }

    assert.doesNotThrow(() => validate(members));
});

test("minimal receiver still rejects Package schema and kbVersion drift", () => {
    for (const mutate of [
        (manifest) => { manifest.schemaVersion = "3.0"; },
        (manifest) => { manifest.kbVersion = "release-latest"; },
    ]) rejects(mutateJsonMember(createPackageV4Members(), "manifest.json", mutate));
});

test("minimal receiver retains checksum and runtime reference closure", () => {
    const checksumMismatch = createPackageV4Members();
    checksumMismatch.set("chunks.jsonl", Buffer.concat([
        checksumMismatch.get("chunks.jsonl"),
        Buffer.from(" "),
    ]));
    assert.throws(() => validate(checksumMismatch), errorCode("checksum_mismatch"));

    rejects(mutateJsonRecords(
        createPackageV4Members(),
        "chunks.jsonl",
        (chunk) => { chunk.docId = `d_${"0".repeat(64)}`; },
    ));
});

test("minimal receiver retains vector and BM25 runtime safety", () => {
    const invalidVector = createPackageV4Members();
    const vector = Buffer.from(invalidVector.get("vectors.f32"));
    vector.writeFloatLE(Number.NaN, 0);
    invalidVector.set("vectors.f32", vector);
    rewriteChecksums(invalidVector);
    rejects(invalidVector);

    rejects(mutateJsonMember(createPackageV4Members(), "bm25.json", (bm25) => {
        bm25.terms = { unsafe: { idf: 1.0, postings: [[99, 1]] } };
    }));
});

test("Package owns the canonical BM25 domain dictionary", () => {
    const members = createPackageV4Members();
    assert.equal(validate(members).domainDictionary, DOMAIN_DICTIONARY_TEXT);

    const digestMismatch = createPackageV4Members();
    digestMismatch.set("bm25-domain-dict.txt", Buffer.from("different term\n", "utf8"));
    rewriteChecksums(digestMismatch);
    assert.throws(() => validate(digestMismatch), errorCode("checksum_mismatch"));

    const duplicateTerms = createPackageV4Members();
    const dictionary = Buffer.from("memory analysis\nmemory analysis\n", "utf8");
    duplicateTerms.set("bm25-domain-dict.txt", dictionary);
    mutateJsonMember(duplicateTerms, "manifest.json", (manifest) => {
        manifest.retrieval.keyword.domainDictionarySha256 = sha256(dictionary);
    });
    rejects(duplicateTerms);
});

test("minimal receiver rejects absent runtime/model contracts and malformed member closure", () => {
    const members = createPackageV4Members();
    assert.throws(
        () => validatePackageMemberBytes({ members, runtimeContract: undefined, modelContract: MODEL_CONTRACT }),
        errorCode("model_contract_mismatch"),
    );
    assert.throws(() => validatePackageMemberBytes({ members: {}, runtimeContract: RUNTIME_CONTRACT, modelContract: MODEL_CONTRACT }));
    const missing = createPackageV4Members();
    missing.delete("sources.jsonl");
    assert.throws(() => validate(missing));
});

test("minimal receiver rejects invalid source paths, duplicate provenance, and chunk provenance", () => {
    rejects(mutateJsonRecords(createPackageV4Members(), "documents.jsonl", (document) => {
        document.sourcePath = "../escape.md";
    }));
    rejects(mutateJsonRecords(createPackageV4Members(), "documents.jsonl", (document) => {
        document.duplicateOfDocId = document.docId;
    }));
    rejects(mutateJsonRecords(createPackageV4Members(), "chunks.jsonl", (chunk) => {
        chunk.projectId = "wrong-project";
    }));
});

test("minimal receiver rejects partial and inconsistent FAQ fields", () => {
    for (const mutate of [
        firstChunk((chunk) => { chunk.answerStatus = "answered"; }),
        firstChunk((chunk) => Object.assign(chunk, {
            answerStatus: "unknown",
            faqId: `q_${"1".repeat(64)}`,
            faqPartIndex: 0,
        })),
        firstChunk((chunk) => Object.assign(chunk, {
            answerStatus: "answered",
            faqId: `q_${"1".repeat(64)}`,
            faqPartIndex: 2,
        })),
    ]) rejects(mutateJsonRecords(createPackageV4Members(), "chunks.jsonl", mutate));
});

test("minimal receiver rejects vector shape and BM25 ordering or averages", () => {
    const shortVector = createPackageV4Members();
    shortVector.set("vectors.f32", shortVector.get("vectors.f32").subarray(4));
    rewriteChecksums(shortVector);
    rejects(shortVector);

    for (const mutate of [
        (bm25) => { bm25.avgDocLength += 1; },
        (bm25) => { bm25.terms = { unsafe: { idf: 1, postings: [[0, 1], [0, 1]] } }; },
        (bm25) => { bm25.docLengths = []; },
    ]) rejects(mutateJsonMember(createPackageV4Members(), "bm25.json", mutate));
});

const validate = (members) => validatePackageMemberBytes({
    members,
    runtimeContract: RUNTIME_CONTRACT,
    modelContract: MODEL_CONTRACT,
});

const rejects = (members) => assert.throws(
    () => validate(members),
    (error) => [
        "package_semantics_invalid",
        "unsupported_package_schema",
        "model_contract_mismatch",
    ].includes(error?.code),
);

const mutateJsonMember = (members, name, mutate) => {
    const payload = JSON.parse(members.get(name).toString("utf8"));
    mutate(payload);
    members.set(name, canonicalJsonBytes(payload));
    rewriteChecksums(members);
    return members;
};

const mutateJsonRecords = (members, name, mutate) => {
    const records = members
        .get(name)
        .toString("utf8")
        .trimEnd()
        .split("\n")
        .map((row) => JSON.parse(row));
    for (const record of records) mutate(record);
    members.set(name, Buffer.concat(records.map(canonicalJsonBytes)));
    rewriteChecksums(members);
    return members;
};

const rewriteChecksums = (members) => {
    members.set("checksums.json", canonicalJsonBytes({
        schemaVersion: "4.0",
        algorithm: "sha256",
        files: Object.fromEntries(CHECKSUM_MEMBERS.map((name) => [name, sha256(members.get(name))])),
    }));
};

const errorCode = (code) => (error) => {
    assert.equal(error?.code, code, error?.stack);
    return true;
};

const firstChunk = (mutate) => {
    let seen = false;
    return (chunk) => {
        if (seen) return;
        seen = true;
        mutate(chunk);
    };
};
