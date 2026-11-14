-- Выполнить один раз в SQL Editor своего проекта Supabase; повторный запуск не поддерживается.
-- Используется только публичный publishable key. Service role в браузер не помещать.
begin;
create table public.профили (
  id uuid primary key references auth.users(id) on delete cascade,
  имя text not null check (char_length(имя) between 1 and 80)
);
create table public.программы (
  id uuid primary key default gen_random_uuid(),
  автор uuid not null default auth.uid() references auth.users(id) on delete cascade,
  название text not null check (char_length(trim(название)) between 1 and 100),
  исходник text not null check (octet_length(исходник) between 1 and 32768),
  создано timestamptz not null default now(),
  скрыто boolean not null default false
);
create table public.темы (
  id uuid primary key default gen_random_uuid(),
  автор uuid not null default auth.uid() references auth.users(id) on delete cascade,
  название text not null check (char_length(trim(название)) between 1 and 100),
  текст text not null check (char_length(trim(текст)) between 1 and 5000),
  создано timestamptz not null default now(),
  скрыто boolean not null default false
);
-- Раздельные таблицы дают настоящие внешние ключи с каскадным удалением.
create table public.жалобы_программы (
  автор uuid not null default auth.uid() references auth.users(id) on delete cascade,
  цель uuid not null references public.программы(id) on delete cascade,
  создано timestamptz not null default now(), primary key (автор, цель)
);
create table public.жалобы_темы (
  автор uuid not null default auth.uid() references auth.users(id) on delete cascade,
  цель uuid not null references public.темы(id) on delete cascade,
  создано timestamptz not null default now(), primary key (автор, цель)
);
create function public.создать_профиль() returns trigger language plpgsql security definer set search_path = '' as $$
begin
  insert into public.профили(id, имя) values (new.id, left(coalesce(nullif(new.raw_user_meta_data->>'user_name',''), 'Участник'), 80)) on conflict (id) do nothing;
  return new;
end $$;
create trigger профиль_пользователя after insert on auth.users for each row execute function public.создать_профиль();
insert into public.профили(id, имя) select id, left(coalesce(nullif(raw_user_meta_data->>'user_name',''), 'Участник'),80) from auth.users on conflict (id) do nothing;
create function public.проверить_жалобы() returns trigger language plpgsql security definer set search_path = '' as $$
begin
  if tg_table_name = 'жалобы_программы' then
    perform 1 from public.программы where id = new.цель for no key update;
    if (select count(*) from public.жалобы_программы where цель = new.цель) >= 5 then
      update public.программы set скрыто = true where id = new.цель;
    end if;
  else
    perform 1 from public.темы where id = new.цель for no key update;
    if (select count(*) from public.жалобы_темы where цель = new.цель) >= 5 then
      update public.темы set скрыто = true where id = new.цель;
    end if;
  end if;
  return new;
end $$;
create trigger пять_жалоб_программы after insert on public.жалобы_программы for each row execute function public.проверить_жалобы();
create trigger пять_жалоб_темы after insert on public.жалобы_темы for each row execute function public.проверить_жалобы();
revoke all on function public.создать_профиль() from public, anon, authenticated;
revoke all on function public.проверить_жалобы() from public, anon, authenticated;
alter table public.профили enable row level security;
alter table public.программы enable row level security;
alter table public.темы enable row level security;
alter table public.жалобы_программы enable row level security;
alter table public.жалобы_темы enable row level security;
revoke all on public.профили, public.программы, public.темы, public.жалобы_программы, public.жалобы_темы from anon, authenticated;
grant select on public.профили, public.программы, public.темы to anon, authenticated;
grant insert (автор, название, исходник) on public.программы to authenticated;
grant insert (автор, название, текст) on public.темы to authenticated;
grant insert (автор, цель) on public.жалобы_программы, public.жалобы_темы to authenticated;
create policy профили_чтение on public.профили for select to anon, authenticated using (true);
create policy программы_чтение on public.программы for select to anon, authenticated using (not скрыто);
create policy темы_чтение on public.темы for select to anon, authenticated using (not скрыто);
create policy программы_публикация on public.программы for insert to authenticated with check (автор = (select auth.uid()) and (select auth.jwt()->'app_metadata'->>'provider') = 'github' and not скрыто);
create policy темы_публикация on public.темы for insert to authenticated with check (автор = (select auth.uid()) and (select auth.jwt()->'app_metadata'->>'provider') = 'github' and not скрыто);
create policy жалобы_программы_добавление on public.жалобы_программы for insert to authenticated with check (
  автор = (select auth.uid()) and (select auth.jwt()->'app_metadata'->>'provider') = 'github' and exists (select 1 from public.программы p where p.id = цель and not p.скрыто and p.автор <> (select auth.uid()))
);
create policy жалобы_темы_добавление on public.жалобы_темы for insert to authenticated with check (
  автор = (select auth.uid()) and (select auth.jwt()->'app_metadata'->>'provider') = 'github' and exists (select 1 from public.темы t where t.id = цель and not t.скрыто and t.автор <> (select auth.uid()))
);
commit;
