#!/usr/bin/env bash
set -e

cat <<EOF
MindStudio Insight ${ASCEND_SOFTWARE_VERSION}
Container image Copyright (c) 2026, Huawei Technologies Co., Ltd. All rights reserved.

This container image and its contents are governed by the Huawei Container License Agreement ("License"). By pulling and using the container, you accept the terms and conditions of this License.
A copy of this License is made available in this container at: https://www.hiascend.com/en/legal/ascendhub-download

Note: You agree and undertake that when using Huawei or third-party software in this image, you will comply with the license agreement of the corresponding Huawei or third-party software.
EOF

echo "=== MindStudio Insight Container Starting ==="
echo "Architecture: $(uname -m)"

# ---- Detect TLS mode ----
NGINX_TEMPLATE_DIR="/etc/nginx/templates"
NGINX_ACTIVE_CONF="/etc/nginx/conf.d/insight.conf"

if [ -f /etc/nginx/certs/server.crt ] && [ -f /etc/nginx/certs/server.key ] && [ -f /etc/nginx/certs/ca.crt ]; then
    echo "TLS mode: HTTPS + mTLS (certificates detected)"
    cp "${NGINX_TEMPLATE_DIR}/nginx.conf" "${NGINX_ACTIVE_CONF}"
else
    echo "TLS mode: HTTP (no certificates mounted)"
    cp "${NGINX_TEMPLATE_DIR}/nginx-http.conf" "${NGINX_ACTIVE_CONF}"
fi

# ---- Verify backend binary ----
if [ ! -x /opt/insight/backend/profiler_server ]; then
    echo "ERROR: Backend binary not found or not executable: /opt/insight/backend/profiler_server" >&2
    exit 1
fi

# ---- Ensure log directories ----
mkdir -p /var/log/nginx /var/log/supervisor

# ---- Validate nginx configuration ----
nginx -t 2>&1 || { echo "ERROR: nginx config test failed" >&2; exit 1; }

echo "All checks passed. Starting services..."

# ---- Pass through to CMD ----
exec "$@"
