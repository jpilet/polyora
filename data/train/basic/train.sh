#!/bin/bash

set -e

SOURCES="${BASE}/data/sources/*.ini"
TREE_DEPTH=9
MIN_TRAINING_SAMPLES=64

BASE="../../.."
BUILD="${BASE}/build"

QTPOLYORA="${BUILD}/qtpolyora/qtpolyora"
BUILDTREE="${BUILD}/buildtree/buildtree"
CLUSTERIDS="${BUILD}/cluster_ids/cluster_ids"

DESCRIPTORS="pts_descriptors.dat"
TRACKS="tracks.dat"

function gather_pts_descriptors() {
  rm -f "${DESCRIPTORS}"
  for i in ${SOURCES}; do
    "${QTPOLYORA}" -rp -d "${DESCRIPTORS}" -vs "$i"
  done
}

function compute_kmean_tree() {
  ${BUILDTREE} -r ${TREE_DEPTH} -e ${MIN_TRAINING_SAMPLES} \
      -d "${DESCRIPTORS}" -v visual.db
}

function gather_tracks() {
  rm -f "${TRACKS}"
  for i in ${SOURCES}; do
    "${QTPOLYORA}" -rt -d "${TRACKS}" -vs "$i" -v visual.db
  done
}

function cluster_ids() {
  "${CLUSTERIDS}" -T .5 -C visual.db -v visual.db
  "${CLUSTERIDS}" -T .3 -C visual.db -v visual.db
  "${CLUSTERIDS}" -T .1 -C visual.db -v visual.db
}

function all() {
  gather_pts_descriptors
  compute_kmean_tree
  gather_tracks
  cluster_ids
}

$1
