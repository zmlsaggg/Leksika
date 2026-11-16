-- Дополнение к схеме.sql. Выполнить один раз после исходной схемы.
begin;
create table public.ответы_темы (
  id uuid primary key default gen_random_uuid(),
  тема uuid not null references public.темы(id) on delete cascade,
  автор uuid not null default auth.uid() references auth.users(id) on delete cascade,
  текст text not null check (char_length(trim(текст)) between 1 and 2000),
  создано timestamptz not null default now()
);
create index on public.ответы_темы (тема, создано, id);
create table public.голоса_темы (
  тема uuid not null references public.темы(id) on delete cascade,
  автор uuid not null references auth.users(id) on delete cascade,
  оценка smallint not null check (оценка in (-1, 1)),
  primary key (тема, автор)
);
alter table public.ответы_темы enable row level security;
alter table public.голоса_темы enable row level security;
revoke all on public.ответы_темы, public.голоса_темы from public, anon, authenticated;
grant select on public.ответы_темы to anon, authenticated;
grant insert (тема, автор, текст) on public.ответы_темы to authenticated;
create policy ответы_чтение on public.ответы_темы for select to anon, authenticated
using (exists (select 1 from public.темы t where t.id = тема and not t.скрыто));
create policy ответы_добавление on public.ответы_темы for insert to authenticated
with check (автор = (select auth.uid()) and (select auth.jwt()->'app_metadata'->>'provider') = 'github'
  and exists (select 1 from public.темы t where t.id = тема and not t.скрыто));

-- Агрегаты не раскрывают отдельные голоса и не зависят от ограничения выдачи API.
create function public.оценки_тем(темы uuid[])
returns table (тема uuid, счёт bigint, моя smallint)
language sql stable security definer set search_path = '' as $$
  select t.id, coalesce(sum(g.оценка), 0)::bigint,
    coalesce(max(g.оценка) filter (where g.автор = auth.uid()), 0)::smallint
  from public.темы t left join public.голоса_темы g on g.тема = t.id
  where not t.скрыто and t.id = any(темы)
  group by t.id
$$;
create function public.оценить_тему(тема uuid, значение smallint) returns void
language plpgsql security definer set search_path = '' as $$
declare uid uuid := auth.uid(); предыдущая smallint;
begin
  if uid is null or coalesce(auth.jwt()->'app_metadata'->>'provider', '') <> 'github' then
    raise exception 'Требуется вход через GitHub' using errcode = '42501';
  end if;
  if значение is null or значение not in (-1, 1) then raise exception 'Недопустимая оценка'; end if;
  -- Блокировка темы сериализует повторы и одновременно защищает от скрытия.
  perform 1 from public.темы t where t.id = тема and not t.скрыто for update;
  if not found then raise exception 'Тема недоступна'; end if;
  select g.оценка into предыдущая from public.голоса_темы g where g.тема = оценить_тему.тема and g.автор = uid;
  if предыдущая = значение then
    delete from public.голоса_темы g where g.тема = оценить_тему.тема and g.автор = uid;
  else
    insert into public.голоса_темы as g (тема, автор, оценка) values (тема, uid, значение)
    on conflict on constraint голоса_темы_pkey do update set оценка = excluded.оценка;
  end if;
end $$;
revoke all on function public.оценки_тем(uuid[]), public.оценить_тему(uuid, smallint) from public, anon, authenticated;
grant execute on function public.оценки_тем(uuid[]) to anon, authenticated;
grant execute on function public.оценить_тему(uuid, smallint) to authenticated;
commit;
