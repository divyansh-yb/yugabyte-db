package com.yugabyte.yw.common.operator;

import java.util.List;
import lombok.Data;

@com.fasterxml.jackson.annotation.JsonInclude(
    com.fasterxml.jackson.annotation.JsonInclude.Include.NON_NULL)
@com.fasterxml.jackson.annotation.JsonPropertyOrder({
  "universeStatus",
  "sqlEndpoints",
  "cqlEndpoints"
})
@com.fasterxml.jackson.databind.annotation.JsonDeserialize(
    using = com.fasterxml.jackson.databind.JsonDeserializer.None.class)
@javax.annotation.Generated("io.fabric8.java.generator.CRGeneratorRunner")
@Data
public class YBUniverseStatus implements io.fabric8.kubernetes.api.model.KubernetesResource {

  @com.fasterxml.jackson.annotation.JsonProperty("universeStatus")
  @com.fasterxml.jackson.annotation.JsonSetter(nulls = com.fasterxml.jackson.annotation.Nulls.SKIP)
  private String universeStatus;

  @com.fasterxml.jackson.annotation.JsonProperty("sqlEndpoints")
  private List<String> sqlEndpoints;

  @com.fasterxml.jackson.annotation.JsonProperty("cqlEndpoints")
  private List<String> cqlEndpoints;
}
