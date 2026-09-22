<script setup lang="ts">
defineProps<{ page: number; pageCount: number; total: number; busy: boolean }>()
const emit = defineEmits<{ change: [page: number] }>()
</script>
<template>
  <footer class="pagination" aria-label="分页">
    <span>共 {{ total }} 条记录</span>
    <div class="actions">
      <button :disabled="busy || page <= 1" @click="emit('change', page - 1)">上一页</button
      ><span>{{ page }} / {{ pageCount }}</span
      ><button :disabled="busy || page >= pageCount" @click="emit('change', page + 1)">
        下一页
      </button>
    </div>
  </footer>
</template>
