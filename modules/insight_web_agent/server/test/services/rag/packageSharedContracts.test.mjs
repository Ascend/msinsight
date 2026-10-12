/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

import assert from "node:assert/strict";
import { test } from "node:test";
import { canonicalJsonBytes } from "../../../services/rag/wire/canonicalJson.mjs";
import {
    validateSharedMembers,
} from "../../../services/rag/wire/packageContracts.mjs";
import {
    DOMAIN_DICTIONARY_TEXT,
    MODEL_CONTRACT,
    sha256,
} from "./packageFixture.mjs";
import {
    createPackageV5Members,
    RUNTIME_CONTRACT_V5,
} from "./packageBundleFixture.mjs";

const parseJsonl = (bytes) => bytes.toString("utf8").trimEnd().split("\n").map((row) => JSON.parse(row));

const validate = (members, overrides = {}) => validateSharedMembers({
    manifest: JSON.parse(members.get("manifest.json").toString("utf8")),
    sources: parseJsonl(members.get("sources.jsonl")),
    documents: parseJsonl(members.get("documents.jsonl")),
    chunks: parseJsonl(members.get("chunks.jsonl")),
    vectors: members.get("vectors.f32"),
    domainDictionaryBytes: members.get("bm25-domain-dict.txt"),
    bm25: JSON.parse(members.get("bm25.json").toString("utf8")),
    runtimeContract: RUNTIME_CONTRACT_V5,
    modelContract: MODEL_CONTRACT,
    expectedSchemaVersion: "5.0",
    runtimeSchemaVersions: ["5.0"],
    ...overrides,
});

const rejects = (members, overrides) => assert.throws(
    () => validate(members, overrides),
    (error) => [
        "package_semantics_invalid",
        "unsupported_package_schema",
        "model_contract_mismatch",
        "package_contract_unsupported",
        // A BOM is normalized away by UTF-8 decoding, so a BOM-prefixed
        // dictionary fails later at the manifest digest gate instead.
        "checksum_mismatch",
    ].includes(error?.code),
);

const mutateJsonMember = (members, name, mutate) => {
    const payload = JSON.parse(members.get(name).toString("utf8"));
    mutate(payload);
    members.set(name, canonicalJsonBytes(payload));
    return members;
};

const mutateJsonRecords = (members, name, mutate) => {
    const records = parseJsonl(members.get(name));
    for (const record of records) mutate(record);
    members.set(name, Buffer.concat(records.map(canonicalJsonBytes)));
    return members;
};

const firstChunk = (mutate) => {
    let seen = false;
    return (chunk) => {
        if (seen) return;
        seen = true;
        mutate(chunk);
    };
};

test("shared members accept producer metadata drift and build document indexes", () => {
    const members = createPackageV5Members();
    mutateJsonMember(members, "manifest.json", (manifest) => {
        manifest.summary.model = "future-provider/future-model";
        manifest.summary.promptVersion = "future-prompt";
    });
    for (const name of ["manifest.json", "sources.jsonl", "documents.jsonl", "chunks.jsonl"]) {
        if (name.endsWith(".jsonl")) {
            mutateJsonRecords(members, name, (value) => { value.futureProducerMetadata = { accepted: true }; });
        } else {
            mutateJsonMember(members, name, (value) => { value.futureProducerMetadata = { accepted: true }; });
        }
    }

    const pack = validate(members);

    assert.equal(pack.manifest.kbVersion, "26.1.1");
    assert.equal(pack.domainDictionary, DOMAIN_DICTIONARY_TEXT);
    assert.equal(pack.sourceById.size, 1);
    assert.equal(pack.documentById.size, 1);
    assert.equal(pack.chunkById.size, 1);
});

test("shared members reject schema, kbVersion, and runtime gates", () => {
    for (const mutate of [
        (manifest) => { manifest.schemaVersion = "3.0"; },
        (manifest) => { manifest.kbVersion = "release-latest"; },
        (manifest) => { manifest.retrieval.keyword.tokenizer = "other-tokenizer"; },
    ]) rejects(mutateJsonMember(createPackageV5Members(), "manifest.json", mutate));
    rejects(createPackageV5Members(), { runtimeSchemaVersions: ["9.9"] });
    rejects(createPackageV5Members(), { runtimeContract: undefined });
    rejects(createPackageV5Members(), { modelContract: undefined });
});

test("shared members reject noncanonical domain dictionaries", () => {
    const badEncodings = [
        Buffer.from([0xff, 0xfe]),
        Buffer.from("\uFEFFterm\n", "utf8"),
        Buffer.from("term\r\n", "utf8"),
        Buffer.from("term\0\n", "utf8"),
        Buffer.from("term", "utf8"),
        Buffer.alloc(0),
    ];
    for (const bytes of badEncodings) {
        const members = createPackageV5Members();
        members.set("bm25-domain-dict.txt", bytes);
        rejects(members);
    }
    const duplicate = createPackageV5Members();
    const dictionary = Buffer.from("memory analysis\nmemory analysis\n", "utf8");
    duplicate.set("bm25-domain-dict.txt", dictionary);
    mutateJsonMember(duplicate, "manifest.json", (manifest) => {
        manifest.retrieval.keyword.domainDictionarySha256 = sha256(dictionary);
    });
    rejects(duplicate);
});

test("shared members reject invalid source paths, duplicate provenance, and chunk provenance", () => {
    rejects(mutateJsonRecords(createPackageV5Members(), "documents.jsonl", (document) => {
        document.sourcePath = "../escape.md";
    }));
    rejects(mutateJsonRecords(createPackageV5Members(), "documents.jsonl", (document) => {
        document.sourcePath = "docs//double.md";
    }));
    rejects(mutateJsonRecords(createPackageV5Members(), "documents.jsonl", (document) => {
        document.duplicateOfDocId = document.docId;
    }));
    rejects(mutateJsonRecords(createPackageV5Members(), "chunks.jsonl", (chunk) => {
        chunk.projectId = "wrong-project";
    }));
    rejects(mutateJsonRecords(createPackageV5Members(), "chunks.jsonl", (chunk) => {
        chunk.docName = "nested/name.md";
    }));
});

test("shared members reject partial and inconsistent FAQ fields", () => {
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
    ]) rejects(mutateJsonRecords(createPackageV5Members(), "chunks.jsonl", mutate));
});

test("shared members reject vector shape, BM25 ordering, and averages", () => {
    const shortVector = createPackageV5Members();
    shortVector.set("vectors.f32", shortVector.get("vectors.f32").subarray(4));
    rejects(shortVector);

    const nonFinite = createPackageV5Members();
    const vector = Buffer.from(nonFinite.get("vectors.f32"));
    vector.writeFloatLE(Number.NaN, 0);
    nonFinite.set("vectors.f32", vector);
    rejects(nonFinite);

    for (const mutate of [
        (bm25) => { bm25.avgDocLength += 1; },
        (bm25) => { bm25.terms = { unsafe: { idf: 1, postings: [[0, 1], [0, 1]] } }; },
        (bm25) => { bm25.terms = { unsafe: { idf: 1, postings: [[99, 1]] } }; },
        (bm25) => { bm25.terms = { "": { idf: 1, postings: [] } }; },
        (bm25) => { bm25.docLengths = []; },
    ]) rejects(mutateJsonMember(createPackageV5Members(), "bm25.json", mutate));
});
