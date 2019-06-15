FROM gcr.io/kythe-public/bazel-extractor

ENTRYPOINT ["/kythe/fix_permissions.sh"]
